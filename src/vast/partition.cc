#include "vast/partition.h"

#include <cppa/cppa.hpp>
#include "vast/bitmap_index.h"
#include "vast/event.h"
#include "vast/segment.h"
#include "vast/io/serialization.h"

namespace vast {

path const partition::part_meta_file = "partition.meta";

namespace {

// If the given predicate is a name extractor, extracts the name.
struct name_checker : expr::default_const_visitor
{
  virtual void visit(expr::predicate const& pred)
  {
    pred.lhs().accept(*this);
    if (check_)
      pred.rhs().accept(*this);
  }

  virtual void visit(expr::name_extractor const&)
  {
    check_ = true;
  }

  virtual void visit(expr::constant const& c)
  {
    value_ = c.val;
  }

  bool check_ = false;
  value value_;
};

// Partitions the AST predicates into meta and data predicates. Moreover, it
// assigns each data predicate a name if a it coexists with name-extractor
// predicate that restricts the scope of the data predicate.
struct predicatizer : expr::default_const_visitor
{
  virtual void visit(expr::conjunction const& conj)
  {
    std::vector<expr::ast> flat;
    std::vector<expr::ast> deep;

    // Partition terms into leaves and others.
    for (auto& operand : conj.operands)
    {
      auto op = expr::ast{*operand};
      if (op.is_predicate())
        flat.push_back(std::move(op));
      else
        deep.push_back(std::move(op));
    }

    // Extract the name for all predicates in this conjunction.
    //
    // TODO: add a check to the semantic pass after constructing queries to
    // flag multiple name/offset/time extractors in the same conjunction.
    name_.clear();
    for (auto& pred : flat)
    {
      name_checker checker;
      pred.accept(checker);
      if (checker.check_)
        name_ = checker.value_.get<string>();
    }

    for (auto& pred : flat)
      pred.accept(*this);

    // Proceed with the remaining conjunctions deeper in the tree.
    name_.clear();
    for (auto& n : deep)
      n.accept(*this);
  }

  virtual void visit(expr::disjunction const& disj)
  {
    for (auto& operand : disj.operands)
      operand->accept(*this);
  }

  virtual void visit(expr::predicate const& pred)
  {
    if (! expr::ast{pred}.is_meta_predicate())
      data_predicates_.emplace(name_, pred);
    else
      meta_predicates_.push_back(pred);
  }

  string name_;
  std::multimap<string, expr::ast> data_predicates_;
  std::vector<expr::ast> meta_predicates_;
};

} // namespace <anonymous>

partition::meta_data::meta_data(uuid id)
  : id{id}
{
}

void partition::meta_data::update(segment const& s)
{
  if (first_event == time_range{} || s.first() < first_event)
    first_event = s.first();

  if (last_event == time_range{} || s.last() > last_event)
    last_event = s.last();

  last_modified = now();
}

void partition::meta_data::serialize(serializer& sink) const
{
  sink << id << first_event << last_event << last_modified;
}

void partition::meta_data::deserialize(deserializer& source)
{
  source >> id >> first_event >> last_event >> last_modified;
}

bool operator==(partition::meta_data const& x, partition::meta_data const& y)
{
  return x.id == y.id
      && x.first_event == y.first_event
      && x.last_event == y.last_event
      && x.last_modified == y.last_modified;
}


partition::partition(uuid id)
  : meta_{std::move(id)}
{
}

void partition::update(segment const& s)
{
  meta_.update(s);
}

partition::meta_data const& partition::meta() const
{
  return meta_;
}

void partition::serialize(serializer& sink) const
{
  sink << meta_;
}

void partition::deserialize(deserializer& source)
{
  source >> meta_;
}


using namespace cppa;

namespace {

struct dispatcher : expr::default_const_visitor
{
  dispatcher(partition_actor& pa)
    : actor_{pa}
  {
  }

  virtual void visit(expr::predicate const& pred)
  {
    pred.rhs().accept(*this);
    assert(value_);
    pred.lhs().accept(*this);
  }

  virtual void visit(expr::timestamp_extractor const&)
  {
    if (! actor_.time_indexer_)
    {
      auto p = actor_.dir_ / "time.idx";
      actor_.time_indexer_ =
        spawn<event_time_indexer<default_bitstream>>(std::move(p));
    }

    indexes_.push_back(actor_.time_indexer_);
  }

  virtual void visit(expr::name_extractor const&)
  {
    if (! actor_.name_indexer_)
    {
      auto p = actor_.dir_ / "name.idx";
      actor_.name_indexer_ =
        spawn<event_name_indexer<default_bitstream>>(std::move(p));
    }

    indexes_.push_back(actor_.name_indexer_);
  }

  virtual void visit(expr::type_extractor const& te)
  {
    // TODO: Switch to strong typing.
    for (auto& p : actor_.indexers_)
      if (p.second.type->at(p.second.off)->tag() == te.type)
      {
        if (p.second.actor)
        {
          indexes_.push_back(p.second.actor);
        }
        else
        {
          auto a = actor_.load_indexer(p.first);
          if (! a)
            VAST_LOG_ERROR(a.error());
          else
            indexes_.push_back(*a);
        }
      }
  }

  virtual void visit(expr::schema_extractor const& se)
  {
    for (auto& t : actor_.schema_)
      for (auto& pair : t->find_suffix(se.key))
      {
        path p;
        for (auto& i : pair.second)
          p /= i;
        
        auto i = actor_.indexers_.find(p);
        if (i == actor_.indexers_.end())
        {
          VAST_LOG_WARN("no index for " << p);
        }
        else if (i->second.type->at(i->second.off)->tag() != value_->which())
        {
          VAST_LOG_WARN("type mismatch: requested type " << value_->which() <<
                        " but " << p << " has type " <<
                        i->second.type->at(i->second.off)->tag());
        }
        else if (! i->second.actor)
        {
          VAST_LOG_DEBUG("loading indexer for " << p);
          auto a = actor_.load_indexer(i->first);
          if (! a)
            VAST_LOG_ERROR(a.error());
          else
            indexes_.push_back(*a);
        }
        else
        {
          indexes_.push_back(i->second.actor);
        }
      }
  }

  virtual void visit(expr::constant const& c)
  {
    value_ = &c.val;
  }

  value const* value_ = nullptr;
  partition_actor& actor_;
  std::vector<actor_ptr> indexes_;
};

} // namespace <anonymous>

partition_actor::partition_actor(path dir, size_t batch_size, uuid id)
  : dir_{std::move(dir)},
    batch_size_{batch_size},
    partition_{std::move(id)}
{
}

void partition_actor::act()
{
  chaining(false);
  trap_exit(true);

  if (exists(dir_))
  {
    auto t = io::unarchive(dir_ / partition::part_meta_file, partition_);
    if (! t)
    {
      VAST_LOG_ACTOR_ERROR("failed to load partition meta data from " <<
                           dir_ << ": " << t.error().msg());
      quit(exit::error);
      return;
    }

    t = io::unarchive(dir_ / "schema", schema_);
    if (! t)
    {
      VAST_LOG_ACTOR_ERROR("failed to load schema: " << t.error());
      quit(exit::error);
      return;
    }

    for (auto& t : schema_)
      t->each(
          [&](key const& k, offset const& o)
          {
            path p = t->name();
            for (auto& id : k)
              p /= id;

            VAST_LOG_ACTOR_DEBUG("initializing indexer for " << *t << '.' << k);

            assert(indexers_.find(p) == indexers_.end());
            auto& state = indexers_[p];
            state.off = o;
            state.type = t;
          });
  }

  auto submit = [=](std::vector<event> events)
  {
    VAST_LOG_ACTOR_DEBUG("sends " << events.size() << " events to indexers");

    auto t = make_any_tuple(std::move(events));

    if (! name_indexer_)
    {
      auto p = dir_ / "name.idx";
      name_indexer_ =
        spawn<event_name_indexer<default_bitstream>>(std::move(p));
    }

    if (! time_indexer_)
    {
      auto p = dir_ / "time.idx";
      time_indexer_ =
        spawn<event_time_indexer<default_bitstream>>(std::move(p));
    }

    name_indexer_ << t;
    time_indexer_ << t;
    for (auto& p : indexers_)
      if (p.second.actor)
        p.second.actor << t;

  };

  auto flush = [=]
  {
    VAST_LOG_ACTOR_DEBUG("flushes its indexes in " << dir_);

    send(name_indexer_, atom("flush"));
    send(time_indexer_, atom("flush"));
    for (auto& p : indexers_)
      if (p.second.actor)
        send(p.second.actor, atom("flush"));

    auto t = io::archive(dir_ / "schema", schema_);
    if (! t)
    {
      VAST_LOG_ACTOR_ERROR("failed to save schema for " << dir_ << ": " <<
                           t.error().msg());
      quit(exit::error);
      return;
    }

    t = io::archive(dir_ / partition::part_meta_file, partition_);
    if (! t)
    {
      VAST_LOG_ACTOR_ERROR("failed to save partition " << dir_ <<
                           ": " << t.error().msg());
      quit(exit::error);
      return;
    }
  };

  become(
      on(atom("EXIT"), arg_match) >> [=](uint32_t reason)
      {
        if (reason != exit::kill)
          flush();

        send_exit(time_indexer_, reason);
        send_exit(name_indexer_, reason);
        for (auto& p : indexers_)
          if (p.second.actor)
            send_exit(p.second.actor, reason);

        quit(reason);
      },
      on(atom("DOWN"), arg_match) >> [=](uint32_t /* reason */)
      {
        VAST_LOG_ACTOR_DEBUG("got DOWN from " << VAST_ACTOR_ID(last_sender()));

        for (auto& p : indexers_)
          if (p.second.actor == last_sender())
          {
            p.second.actor = {};
            p.second.stats = {};
            break;
          }
      },
      on_arg_match >> [=](expr::ast const& pred, actor_ptr const& sink)
      {
        VAST_LOG_ACTOR_DEBUG("got predicate " << pred <<
                             " for " << VAST_ACTOR_ID(sink));

        dispatcher d{*this};
        pred.accept(d);

        uint64_t n = d.indexes_.size();
        send(last_sender(), pred, partition_.meta().id, n);

        if (n == 0)
        {
          send(last_sender(), pred, partition_.meta().id, bitstream{});
        }
        else
        {
          auto t = make_any_tuple(pred, partition_.meta().id, sink);
          for (auto& a : d.indexes_)
            a << t;
        }
      },
      on(atom("flush")) >> flush,
      on_arg_match >> [=](segment const& s)
      {
        VAST_LOG_ACTOR_VERBOSE(
            "processes " << s.events() << " events from segment " << s.id());

        auto sch = schema::merge(schema_, s.schema());
        if (! sch)
        {
          VAST_LOG_ACTOR_ERROR("failed to merge schemata: " << sch.error());
          VAST_LOG_ACTOR_ERROR("ignoring segment with " << s.events() <<
                               " events");
          return;
        }

        schema_ = std::move(*sch);

        for (auto& t : s.schema())
          t->each(
            [&](key const& k, offset const& o)
            {
              path p = t->name();
              for (auto& id : k)
                p /= id;

              if (indexers_.find(p) != indexers_.end())
                return;

              VAST_LOG_ACTOR_DEBUG("creates indexer for key " << k <<
                                   " at offset " << o << " in type " << *t);
              auto i = create_indexer(p, o, t);
              if (! i)
              {
                VAST_LOG_ACTOR_ERROR(i.error());
                quit(exit::error);
              }
            });

        std::vector<event> events;
        events.reserve(batch_size_);

        segment::reader r{&s};
        while (auto e = r.read())
        {
          assert(e);

          // We only supported fully typed events at this point, but may loosen
          // this constraint in the future.
          auto& t = e->type();
          if (t->name().empty() || util::get<invalid_type>(t->info()))
          {
            VAST_LOG_ACTOR_ERROR("encountered invalid event type: " << *t);
            quit(exit::error);
            return;
          }

          events.push_back(std::move(*e));
          if (events.size() == batch_size_)
          {
            events.shrink_to_fit();
            submit(std::move(events));
            events = {};
          }
        }

        // Send away final events.
        events.shrink_to_fit();
        submit(std::move(events));

        // Record segment.
        partition_.update(s);

        // Flush partition meta data and data indexes.
        flush();
        send(self, atom("stats"), atom("show"));
      },
      on(atom("stats"), arg_match)
        >> [=](uint64_t n, uint64_t rate, uint64_t mean)
      {
        for (auto& p : indexers_)
          if (p.second.actor == last_sender())
          {
            p.second.stats.values += n;
            p.second.stats.rate = rate;
            p.second.stats.mean = mean;
            break;
          }
      },
      on(atom("stats"), atom("show")) >> [=]
      {
        uint64_t total_values = 0;
        uint64_t total_rate = 0;
        uint64_t total_mean = 0;
        for (auto& p : indexers_)
          if (p.second.actor)
          {
            total_values += p.second.stats.values;
            total_rate += p.second.stats.rate;
            total_mean += p.second.stats.mean;
          }

        if (total_rate > 0)
          VAST_LOG_ACTOR_INFO(
              "indexed " << total_values << " values at rate " <<
              total_rate << " values/sec" << " (mean " << total_mean << ')');
      });
}

char const* partition_actor::description() const
{
  return "partition";
}

} // namespace vast
