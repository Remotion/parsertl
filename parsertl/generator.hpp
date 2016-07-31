// generator.hpp
// Copyright (c) 2014-2016 Ben Hanson (http://www.benhanson.net/)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file licence_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef PARSERTL_GENERATOR_HPP
#define PARSERTL_GENERATOR_HPP

#include "dfa.hpp"
#include "narrow.hpp"
#include "nt_state.hpp"
#include <queue>
#include "rules.hpp"
#include "runtime_error.hpp"
#include "state_machine.hpp"

namespace parsertl
{
template<typename rules>
class basic_generator
{
public:
    typedef typename rules::nt_enum_map nt_enum_map;
    typedef typename rules::string string;
    typedef typename rules::symbol_map symbol_map;

    static void build(rules &rules_, state_machine &sm_,
        std::string *warnings_ = 0, size_t_pair_set_deque *core_cfgs_ = 0)
    {
        nt_enum_map nt_enums_;
        size_t_nt_state_map nt_states_;
        size_t_pair_set_deque configs_;
        dfa dfa_;
        prod_deque new_grammar_;
        rules new_rules_;
        nt_enum_map new_nt_enums_;
        size_t_nt_state_map new_nt_states_;

        sm_.clear();
        rules_.validate();
        enumerate_non_terminals(rules_, nt_enums_);
        fill_nt_states(nt_enums_, nt_states_, rules_.terminals().size());
        build_first_sets(rules_, nt_enums_, nt_states_);
        build_dfa(rules_, nt_enums_, core_cfgs_, configs_, dfa_);

        rewrite(rules_, nt_enums_, configs_, dfa_, new_grammar_, new_rules_);
        enumerate_non_terminals(new_rules_, new_nt_enums_);
        fill_nt_states(new_nt_enums_, new_nt_states_,
            new_rules_.terminals().size());
        // First add EOF to follow_set of start.
        new_nt_states_.find(new_nt_enums_.find
           (new_rules_.start())->second)->second._follow_set[0] = 1;
        build_first_sets(new_rules_, new_nt_enums_, new_nt_states_);
        build_follow_sets(new_rules_, new_nt_enums_, new_nt_states_);
        build_table(rules_, nt_enums_, nt_states_, configs_, dfa_,
            new_grammar_, new_rules_, new_nt_enums_, new_nt_states_, sm_,
            warnings_);
        copy_rules(rules_, nt_enums_, sm_);
    }
/*
    static void build_dfa(const rules &rules_, const nt_enum_map &nt_enums_,
        size_t_pair_set_deque *core_cfgs_, size_t_pair_set_deque &configs_,
        dfa &dfa_)
    {
        std::size_t index_ = 0;
        int_grammar grammar_;
        int_non_terminal_map non_terminals_;
        const terminal_map &terminals_ = rules_.terminals();
        typedef std::pair<std::size_t, size_t_pair_set> sym_set_pair;
        typedef std::map<std::size_t, size_t_pair_set> sym_set_map;
        // Not owner
        std::queue<size_t_pair_set *> queue_;

        grammar_.resize(rules_.grammar().size());

        for (typename grammar::const_iterator iter_ =
            rules_.grammar().begin(), end_ = rules_.grammar().end();
            iter_ != end_; ++iter_, ++index_)
        {
            std::size_t rhs_idx_ = 0;

            grammar_[index_]._lhs =
                nt_enums_.find(iter_->_symbols._lhs)->second;
            grammar_[index_]._rhs.resize(iter_->_symbols._rhs.size());

            for (typename symbol_deque::const_iterator i_ =
                iter_->_symbols._rhs.begin(), e_ = iter_->_symbols._rhs.end();
                i_ != e_; ++i_, ++rhs_idx_)
            {
                switch (i_->_type)
                {
                case symbol::TERMINAL:
                    grammar_[index_]._rhs[rhs_idx_] =
                        terminals_.find(i_->_name)->second._id;
                    break;
                case symbol::NON_TERMINAL:
                    grammar_[index_]._rhs[rhs_idx_] =
                        nt_enums_.find(i_->_name)->second;
                    break;
                }
            }

            grammar_[index_]._next_lhs = iter_->_next_lhs;
        }

        for (typename non_terminal_map::const_iterator iter_ =
            rules_.non_terminals().begin(),
            end_ = rules_.non_terminals().end(); iter_ != end_; ++iter_)
        {
            non_terminals_[nt_enums_.find(iter_->first)->second] =
                iter_->second;
        }

        configs_.push_back(size_t_pair_set());
        closure(grammar_, terminals_.size() - 1, non_terminals_,
            size_t_pair(rules_.grammar()
                [rules_.non_terminals().find(rules_.start())->
                second._first_production]._index, 0), configs_.back());
        queue_.push(&configs_.back());

        do
        {
            size_t_pair_set *set_ = queue_.front();
            sym_set_map map_;

            queue_.pop();
            dfa_.push_back(dfa_state());

            dfa_state &state_ = dfa_.back();

            for (typename size_t_pair_set::const_iterator iter_ =
                set_->begin(), end_ = set_->end(); iter_ != end_; ++iter_)
            {
                if (iter_->second < grammar_[iter_->first]._rhs.size())
                {
                    size_t_pair_set set_;
                    const std::size_t id_ =
                        grammar_[iter_->first]._rhs[iter_->second];

                    closure(grammar_, terminals_.size() - 1, non_terminals_,
                        size_t_pair(iter_->first, iter_->second + 1), set_);

                    typename sym_set_map::iterator iter_ = map_.find(id_);

                    if (iter_ == map_.end())
                    {
                        iter_ = map_.insert(sym_set_pair
                            (id_, size_t_pair_set())).first;
                        iter_->second.swap(set_);
                    }
                    else
                    {
                        iter_->second.insert(set_.begin(), set_.end());
                    }
                }
            }

            for (typename sym_set_map::iterator iter_ = map_.begin(),
                end_ = map_.end(); iter_ != end_; ++iter_)
            {
                std::size_t index_ = npos();
                typename size_t_pair_set_deque::const_iterator config_iter_ =
                    std::find(configs_.begin(), configs_.end(), iter_->second);

                if (config_iter_ == configs_.end())
                {
                    index_ = configs_.size();

                    if (core_cfgs_)
                    {
                        core_cfgs_->push_back(size_t_pair_set());
                    }

                    configs_.push_back(size_t_pair_set());
                    configs_.back().insert
                        (iter_->second.begin(), iter_->second.end());
                    queue_.push(&configs_.back());
                }
                else
                {
                    index_ = config_iter_ - configs_.begin();
                }

                typename dfa_state::transition_map::iterator tran_iter_ =
                    state_._transitions.find(iter_->first);

                if (tran_iter_ == state_._transitions.end() ||
                    tran_iter_->second != index_)
                {
                    state_._transitions.insert(typename dfa_state::
                        transition_pair(iter_->first, index_));
                }
            }
        } while (!queue_.empty());
    }
*/
    static void build_dfa(const rules &rules_, const nt_enum_map &nt_enums_,
        size_t_pair_set_deque *core_cfgs_, size_t_pair_set_deque &configs_,
        dfa &dfa_)
    {
        const non_terminal_map &non_terminals_ = rules_.non_terminals();
        std::map<string, size_t_pair_set> nt_cfg_map_;
        const grammar &grammar_ = rules_.grammar();

        for (typename non_terminal_map::const_iterator iter_ =
            non_terminals_.begin(), end_ = non_terminals_.end();
            iter_ != end_; ++iter_)
        {
            size_t_pair_set &set_ = nt_cfg_map_[iter_->first];

            for (std::size_t index_ = iter_->second._first_production;
                index_ != npos(); index_ = grammar_[index_]._next_lhs)
            {
                closure(rules_, size_t_pair(index_, 0), set_);
            }
        }

        // lookup start
        typename non_terminal_map::const_iterator iter_ =
            non_terminals_.find(rules_.start());
        const production &start_ = grammar_[iter_->second._first_production];
        const terminal_map &terminals_ = rules_.terminals();
        typedef std::pair<std::size_t, size_t_pair_set> sym_set_pair;
        typedef std::map<std::size_t, size_t_pair_set> sym_set_map;
        // Not owner
        std::queue<size_t_pair_set *> queue_;

        if (core_cfgs_)
        {
            core_cfgs_->push_back(size_t_pair_set());
            core_cfgs_->back().insert(size_t_pair(start_._index, 0));
        }

        configs_.push_back(size_t_pair_set());
        closure(rules_, size_t_pair(start_._index, 0), configs_.back());
        queue_.push(&configs_.back());

        do
        {
            size_t_pair_set *set_ = queue_.front();
            sym_set_map map_;

            queue_.pop();
            dfa_.push_back(dfa_state());

            dfa_state &state_ = dfa_.back();

            for (typename size_t_pair_set::const_iterator iter_ =
                set_->begin(), end_ = set_->end(); iter_ != end_; ++iter_)
            {
                if (iter_->second < grammar_[iter_->first]._symbols.
                    _rhs.size())
                {
                    size_t_pair_set set_;
                    const symbol &symbol_ =
                        grammar_[iter_->first]._symbols._rhs[iter_->second];
                    const std::size_t id_ = symbol_._type == symbol::TERMINAL ?
                        terminals_.find(symbol_._name)->second._id :
                        nt_enums_.find(symbol_._name)->second;

//                    closure(rules_,
//                        size_t_pair(iter_->first, iter_->second + 1), set_);

                    set_.insert(size_t_pair(iter_->first, iter_->second + 1));

                    if (iter_->second + 1 < grammar_[iter_->first].
                        _symbols._rhs.size())
                    {
                        const symbol &next_symbol_ = grammar_[iter_->first].
                            _symbols._rhs[iter_->second + 1];

                        if (next_symbol_._type == symbol::NON_TERMINAL)
                        {
                            size_t_pair_set &source_ =
                                nt_cfg_map_[next_symbol_._name];

                            set_.insert(source_.begin(), source_.end());
                        }
                    }

                    typename sym_set_map::iterator iter_ = map_.find(id_);

                    if (iter_ == map_.end())
                    {
                        iter_ = map_.insert(sym_set_pair
                            (id_, size_t_pair_set())).first;
                        iter_->second.swap(set_);
                    }
                    else
                    {
                        iter_->second.insert(set_.begin(), set_.end());
                    }
                }
            }

            for (typename sym_set_map::iterator iter_ = map_.begin(),
                end_ = map_.end(); iter_ != end_; ++iter_)
            {
                std::size_t index_ = npos();
                typename size_t_pair_set_deque::const_iterator config_iter_ =
                    std::find(configs_.begin(), configs_.end(), iter_->second);

                if (config_iter_ == configs_.end())
                {
                    index_ = configs_.size();

                    if (core_cfgs_)
                    {
                        core_cfgs_->push_back(size_t_pair_set());
                    }

                    configs_.push_back(size_t_pair_set());
                    configs_.back().insert
                        (iter_->second.begin(), iter_->second.end());
                    queue_.push(&configs_.back());
                }
                else
                {
                    index_ = config_iter_ - configs_.begin();
                }

                typename dfa_state::transition_map::iterator tran_iter_ =
                    state_._transitions.find(iter_->first);

                if (tran_iter_ == state_._transitions.end() ||
                    tran_iter_->second != index_)
                {
                    state_._transitions.insert(typename dfa_state::
                        transition_pair(iter_->first, index_));
                }
            }
        } while (!queue_.empty());

        if (core_cfgs_)
        {
            symbol_map symbols_;

            rules_.symbols(symbols_);

            for (std::size_t i_ = 0, size_ = dfa_.size(); i_ != size_; ++i_)
            {
                typename dfa_state::transition_map::const_iterator iter_ =
                    dfa_[i_]._transitions.begin();
                typename dfa_state::transition_map::const_iterator end_ =
                    dfa_[i_]._transitions.end();

                for (; iter_ != end_; ++iter_)
                {
                    const std::size_t state_ = iter_->second;
                    const size_t_pair_set &set_ = configs_[state_];
                    typename size_t_pair_set::const_iterator set_iter_ =
                        set_.begin();
                    typename size_t_pair_set::const_iterator set_end_ =
                        set_.end();

                    for (; set_iter_ != set_end_; ++set_iter_)
                    {
                        if (set_iter_->second > 0)
                        {
                            const production &production_ =
                                grammar_[set_iter_->first];
                            const symbol &symbol_ =
                                production_._symbols._rhs
                                    [set_iter_->second - 1];

                            if (symbol_._name ==
                                symbols_.find(iter_->first)->second)
                            {
                                (*core_cfgs_)[state_].insert(*set_iter_);
                            }
                        }
                    }
                }
            }
        }
    }

private:
    struct int_prod
    {
        std::size_t _lhs;
        std::vector<std::size_t> _rhs;
        std::size_t _next_lhs;
    };

    typedef typename rules::char_type char_type;
    typedef std::vector<int_prod> int_grammar;
    typedef std::map<std::size_t, typename rules::non_terminal>
        int_non_terminal_map;
    typedef typename rules::lr_symbols lr_symbols;
    typedef typename rules::production_deque grammar;
    typedef std::pair<string, std::size_t> nt_enum_pair;
    typedef typename rules::non_terminal_map non_terminal_map;
    typedef typename rules::production production;
    typedef std::map<std::size_t, std::size_t> size_t_map;
    typedef std::basic_ostringstream<char_type> ostringstream;
    typedef typename rules::symbol symbol;
    typedef typename rules::symbol_deque symbol_deque;
    typedef typename rules::terminal_map terminal_map;
    typedef typename rules::token_info token_info;

    struct prod
    {
        lr_symbols _symbols;
        size_t_pair _lhs_indexes;
        std::vector<size_t_pair> _rhs_indexes;
    };

    typedef std::deque<prod> prod_deque;

    static void enumerate_non_terminals(const rules &rules_,
        nt_enum_map &nt_enums_)
    {
        const grammar &grammar_ = rules_.grammar();
        const std::size_t offset_ = rules_.terminals().size();
        typename grammar::const_iterator iter_ = grammar_.begin();
        typename grammar::const_iterator end_ = grammar_.end();

        nt_enums_.clear();

        for (; iter_ != end_; ++iter_)
        {
            if (nt_enums_.find(iter_->_symbols._lhs) == nt_enums_.end())
            {
                nt_enums_.insert(nt_enum_pair
                    (iter_->_symbols._lhs, nt_enums_.size() + offset_));
            }
        }
    }

    static void closure(const rules &rules_, const size_t_pair &config_,
        size_t_pair_set &set_)
    {
        const grammar &grammar_ = rules_.grammar();
        const non_terminal_map &non_terminals_ = rules_.non_terminals();
        std::queue<size_t_pair> queue_;

        queue_.push(config_);

        do
        {
            size_t_pair curr_ = queue_.front();

            queue_.pop();
            set_.insert(curr_);

            if (curr_.second < grammar_[curr_.first]._symbols._rhs.size())
            {
                const symbol &symbol_ =
                    grammar_[curr_.first]._symbols._rhs[curr_.second];

                if (symbol_._type == symbol::NON_TERMINAL)
                {
                    typename non_terminal_map::const_iterator iter_ =
                        non_terminals_.find(symbol_._name);

                    curr_.second = 0;

                    for (std::size_t index_ = iter_->second._first_production;
                        index_ != npos(); index_ = grammar_[index_]._next_lhs)
                    {
                        curr_.first = index_;

                        if (set_.insert(curr_).second)
                        {
                            queue_.push(curr_);
                        }
                    }
                }
            }
        } while (!queue_.empty());
    }

    static void rewrite(rules &rules_, const nt_enum_map &nt_enums_,
        const size_t_pair_set_deque &configs_, const dfa &dfa_,
        prod_deque &new_grammar_, rules &new_rules_)
    {
        const string &start_ = rules_.start();
        const grammar &grammar_ = rules_.grammar();
        const terminal_map &terminals_ = rules_.terminals();
        typedef std::pair<std::size_t, size_t_pair> trie;
        std::queue<trie> queue_;
        typedef typename size_t_pair_set_deque::const_iterator configs_iter;
        std::size_t index_ = 0;

        // Iterate over all items in all item sets.
        // DFA state, rule, index in rule
        for (configs_iter iter_ = configs_.begin(), end_ = configs_.end();
            iter_ != end_; ++iter_, ++index_)
        {
            typename size_t_pair_set::const_iterator i_ = iter_->begin();
            typename size_t_pair_set::const_iterator e_ = iter_->end();

            for (; i_ != e_; ++i_)
            {
                if (i_->second == 0)
                {
                    queue_.push(trie(index_, *i_));
                }
            }
        }

        for (; !queue_.empty(); queue_.pop())
        {
            trie trie_ = queue_.front();
            const production &production_ = grammar_[trie_.second.first];
            prod *prod_ = 0;

            // Don't include $accept
//            if (production_._symbols._lhs[0] == '$') continue;

            new_grammar_.push_back(prod());
            prod_ = &new_grammar_.back();
            prod_->_symbols = production_._symbols;

            if (production_._symbols._lhs != start_)
            {
                prod_->_lhs_indexes.first = trie_.first;
                prod_->_lhs_indexes.second =
                    dfa_[trie_.first]._transitions.find(nt_enums_.find
                        (production_._symbols._lhs)->second)->second;
            }

            typedef typename symbol_deque::const_iterator rhs_iter;

            index_ = trie_.first;

            if (production_._symbols._rhs.empty())
            {
                prod_->_rhs_indexes.push_back(size_t_pair
                    (prod_->_lhs_indexes.first, prod_->_lhs_indexes.first));
            }
            else
            {
                for (rhs_iter iter_ = production_._symbols._rhs.begin(),
                    end_ = production_._symbols._rhs.end();
                    iter_ != end_; ++iter_)
                {
                    std::size_t id_ = ~0;

                    prod_->_rhs_indexes.push_back(size_t_pair());
                    prod_->_rhs_indexes.back().first = index_;

                    switch (iter_->_type)
                    {
                    case rules::symbol::TERMINAL:
                        id_ = terminals_.find(iter_->_name)->second._id;
                        break;
                    case rules::symbol::NON_TERMINAL:
                        id_ = nt_enums_.find(iter_->_name)->second;
                        break;
                    }

                    index_ = dfa_[index_]._transitions.find(id_)->second;
                    prod_->_rhs_indexes.back().second = index_;
                }
            }
        }

        new_rules(rules_, new_grammar_, new_rules_);
    }

    static void new_rules(const rules &rules_, const prod_deque &new_grammar_,
        rules &new_rules_)
    {
        const string &start_ = rules_.start();
        typename prod_deque::const_iterator iter_ = new_grammar_.begin();
        typename prod_deque::const_iterator end_ = new_grammar_.end();

        rules_.copy_terminals(new_rules_);

        for (; iter_ != end_; ++iter_)
        {
            string lhs_;
            string rhs_;
            const std::size_t size_ = iter_->_symbols._rhs.size();

            if (iter_->_lhs_indexes.first == 0 &&
                iter_->_lhs_indexes.second == 0)
            {
                lhs_ = iter_->_symbols._lhs;
            }
            else
            {
                std::basic_ostringstream<typename rules::char_type> ss_;

                ss_ << iter_->_symbols._lhs << '_' <<
                    iter_->_lhs_indexes.first << '_' <<
                    iter_->_lhs_indexes.second;
                lhs_ = ss_.str();
            }

            for (std::size_t index_ = 0; index_ < size_; ++index_)
            {
                const symbol &symbol_ = iter_->_symbols._rhs[index_];

                if (!rhs_.empty())
                {
                    rhs_ += ' ';
                }

                if (symbol_._type == symbol::TERMINAL ||
                    symbol_._name == start_)
                {
                    if (symbol_._name[0] != '$')
                    {
                        rhs_ += symbol_._name;
                    }
                }
                else
                {
                    ostringstream ss_;

                    ss_ << symbol_._name << '_' <<
                        iter_->_rhs_indexes[index_].first <<
                        '_' << iter_->_rhs_indexes[index_].second;
                    rhs_ += ss_.str();
                }
            }

            if (lhs_[0] != '$')
            {
                new_rules_.push(lhs_.c_str(), rhs_.c_str());
            }
        }

        if (start_[0] != '$')
        {
            new_rules_.start(start_.c_str());
        }

        new_rules_.validate();
    }

    static void fill_nt_states(const nt_enum_map &nt_enums_,
        size_t_nt_state_map &nt_states_, const std::size_t terminals_)
    {
        typename nt_enum_map::const_iterator iter_ = nt_enums_.begin();
        typename nt_enum_map::const_iterator end_ = nt_enums_.end();

        for (; iter_ != end_; ++iter_)
        {
            nt_states_.insert(size_t_nt_state_pair
                (iter_->second, nt_state(terminals_)));
        }
    }

    static void build_first_sets(const rules &rules_,
        const nt_enum_map &nt_enums_, size_t_nt_state_map &nt_states_)
    {
        const grammar &grammar_ = rules_.grammar();
        const terminal_map &terminals_ = rules_.terminals();
        typename grammar::const_iterator iter_;
        typename grammar::const_iterator end_;

        calc_nullable(grammar_, nt_enums_, nt_states_);

        // Start with LHS of productions.
        for (;;)
        {
            bool changes_ = false;

            iter_ = grammar_.begin();
            end_ = grammar_.end();

            for (; iter_ != end_; ++iter_)
            {
                const production &production_ = *iter_;
                typename symbol_deque::const_iterator rhs_iter_ =
                    production_._symbols._rhs.begin();
                typename symbol_deque::const_iterator rhs_end_ =
                    production_._symbols._rhs.end();
                nt_state &lhs_ = nt_states_.find(nt_enums_.find
                    (production_._symbols._lhs)->second)->second;

                for (; rhs_iter_ != rhs_end_; ++rhs_iter_)
                {
                    if (rhs_iter_->_type == symbol::TERMINAL)
                    {
                        const std::size_t id_ = terminals_.find
                            (rhs_iter_->_name)->second._id;

                        if (!lhs_._first_set[id_])
                        {
                            lhs_._first_set[id_] = 1;
                            changes_ = true;
                        }

                        break;
                    }
                    else if (production_._symbols._lhs == rhs_iter_->_name)
                    {
                        if (lhs_._nullable == false)
                            break;
                    }
                    else
                    {
                        nt_state &rhs_ = nt_states_.find
                            (nt_enums_.find(rhs_iter_->_name)->second)->second;

                        changes_ |= set_union(lhs_._first_set, rhs_._first_set);

                        if (rhs_._nullable == false)
                            break;
                    }
                }
            }

            if (!changes_) break;
        }

        // Now process RHS of productions
        for (iter_ = grammar_.begin(), end_ = grammar_.end();
            iter_ != end_; ++iter_)
        {
            const production &production_ = *iter_;
            typename symbol_deque::const_iterator rhs_iter_ =
                production_._symbols._rhs.begin();
            typename symbol_deque::const_iterator rhs_end_ =
                production_._symbols._rhs.end();

            for (; rhs_iter_ != rhs_end_; ++rhs_iter_)
            {
                if (rhs_iter_->_type == symbol::NON_TERMINAL)
                {
                    nt_state *rhs_ = &nt_states_.find(nt_enums_.find
                        (rhs_iter_->_name)->second)->second;
                    typename symbol_deque::const_iterator next_iter_ =
                        rhs_iter_ + 1;

                    for (; rhs_->_nullable && next_iter_ != rhs_end_;
                        ++next_iter_)
                    {
                        if (next_iter_ != rhs_end_)
                        {
                            if (next_iter_->_type == symbol::TERMINAL)
                            {
                                rhs_->_first_set[terminals_.find
                                    (next_iter_->_name)->second._id] = 1;
                                break;
                            }
                            else if (next_iter_->_type == symbol::NON_TERMINAL)
                            {
                                nt_state &next_rhs_ = nt_states_.find
                                    (nt_enums_.find(next_iter_->_name)->
                                    second)->second;

                                set_union(rhs_->_first_set,
                                    next_rhs_._first_set);
                                rhs_ = &nt_states_.find(nt_enums_.find
                                    (next_iter_->_name)->second)->second;
                            }
                        }
                    }

                    if (rhs_->_nullable && next_iter_ == rhs_end_)
                    {
                        nt_states_.find(nt_enums_.find
                            (rhs_iter_->_name)->second)->
                            second._nullable = true;
                    }
                }
            }
        }
    }

    static void calc_nullable(const grammar &grammar_,
        const nt_enum_map &nt_enums_, size_t_nt_state_map &nt_states_)
    {
        for (;;)
        {
            bool changes_ = false;
            typename grammar::const_iterator iter_ = grammar_.begin();
            typename grammar::const_iterator end_ = grammar_.end();

            for (; iter_ != end_; ++iter_)
            {
                const production &production_ = *iter_;
                typename size_t_nt_state_map::iterator state_iter_ =
                    nt_states_.find(nt_enums_.find
                        (production_._symbols._lhs)->second);

                if (state_iter_->second._nullable) continue;

                typename symbol_deque::const_iterator rhs_iter_ =
                    production_._symbols._rhs.begin();
                typename symbol_deque::const_iterator rhs_end_ =
                    production_._symbols._rhs.end();

                for (; rhs_iter_ != rhs_end_; ++rhs_iter_)
                {
                    const symbol &symbol_ = *rhs_iter_;

                    if (symbol_._type != symbol::NON_TERMINAL) break;

                    typename size_t_nt_state_map::iterator state_iter_ =
                        nt_states_.find(nt_enums_.find(symbol_._name)->second);

                    if (state_iter_->second._nullable == false) break;
                }

                if (rhs_iter_ == rhs_end_)
                {
                    state_iter_->second._nullable = true;
                    changes_ = true;
                }
            }

            if (!changes_) break;
        }
    }

    static void build_follow_sets(const rules &rules_,
        const nt_enum_map &nt_enums_, size_t_nt_state_map &nt_states_)
    {
        const grammar &grammar_ = rules_.grammar();
        const terminal_map &terminals_ = rules_.terminals();

        for (;;)
        {
            bool changes_ = false;
            typename grammar::const_iterator iter_ = grammar_.begin();
            typename grammar::const_iterator end_ = grammar_.end();

            for (; iter_ != end_; ++iter_)
            {
                const production &production_ = *iter_;
                const std::size_t lhs_id_ =
                    nt_enums_.find(production_._symbols._lhs)->second;
                typename symbol_deque::const_iterator rhs_iter_ =
                    production_._symbols._rhs.begin();
                typename symbol_deque::const_iterator rhs_end_ =
                    production_._symbols._rhs.end();

                for (; rhs_iter_ != rhs_end_; ++rhs_iter_)
                {
                    if (rhs_iter_->_type == symbol::NON_TERMINAL)
                    {
                        typename symbol_deque::const_iterator next_iter_ =
                            rhs_iter_ + 1;
                        const std::size_t rhs_id_ = nt_enums_.find
                            (rhs_iter_->_name)->second;
                        typename size_t_nt_state_map::iterator
                            lstate_iter_ = nt_states_.find(rhs_id_);
                        bool nullable_ = next_iter_ == rhs_end_;

                        if (next_iter_ != rhs_end_)
                        {
                            if (next_iter_->_type == symbol::TERMINAL)
                            {
                                const std::size_t id_ = terminals_.find
                                    (next_iter_->_name)->second._id;

                                // Just add terminal.
                                if (!lstate_iter_->second._follow_set[id_])
                                {
                                    lstate_iter_->second._follow_set[id_] = 1;
                                    changes_ = true;
                                }
                            }
                            else
                            {
                                // If there is a production A -> aBb
                                // then everything in FIRST(b) is
                                // placed in FOLLOW(B).
                                typename size_t_nt_state_map::const_iterator
                                    rstate_iter_ = nt_states_.find
                                    (nt_enums_.find(next_iter_->_name)->
                                    second);

                                changes_ |= set_union
                                    (lstate_iter_->second._follow_set,
                                    rstate_iter_->second._first_set);
                                ++next_iter_;

                                // If nullable, keep going
                                if (rstate_iter_->second._nullable)
                                {
                                    for (; next_iter_ != rhs_end_; ++next_iter_)
                                    {
                                        std::size_t next_id_ = ~0;

                                        if (next_iter_->_type ==
                                            symbol::TERMINAL)
                                        {
                                            next_id_ = terminals_.find
                                                (next_iter_->_name)->
                                                    second._id;

                                            // Just add terminal.
                                            if (!lstate_iter_->second.
                                                _follow_set[next_id_])
                                            {
                                                lstate_iter_->second._follow_set
                                                    [next_id_] = 1;
                                                changes_ = true;
                                            }

                                            break;
                                        }
                                        else
                                        {
                                            next_id_ = nt_enums_.find
                                                (next_iter_->_name)->second;
                                            rstate_iter_ =
                                                nt_states_.find(next_id_);
                                            changes_ |= set_union
                                                (lstate_iter_->second.
                                                    _follow_set,
                                                rstate_iter_->second.
                                                    _first_set);

                                            if (!rstate_iter_->second._nullable)
                                            {
                                                break;
                                            }
                                        }
                                    }

                                    nullable_ = next_iter_ == rhs_end_;
                                }
                            }
                        }

                        if (nullable_)
                        {
                            // If there is a production A -> aB
                            // then everything in FOLLOW(A) is in FOLLOW(B).
                            typename size_t_nt_state_map::const_iterator
                                rstate_iter_ = nt_states_.find(lhs_id_);

                            changes_ |= set_union
                                (lstate_iter_->second._follow_set,
                                rstate_iter_->second._follow_set);
                        }
                    }
                }
            }

            if (!changes_) break;
        }
    }

    static void build_table(const rules &rules_,
        const nt_enum_map &nt_enums_,
        const size_t_nt_state_map &nt_states_,
        const size_t_pair_set_deque &configs_, const dfa &dfa_,
        const prod_deque &new_grammar_, const rules &new_rules_,
        const nt_enum_map &new_nt_enums_,
        const size_t_nt_state_map &new_nt_states_,
        state_machine &sm_, std::string *warnings_)
    {
        const grammar &grammar_ = rules_.grammar();
        const string &start_ = rules_.start();
        const terminal_map &terminals_ = rules_.terminals();
        symbol_map symbols_;
        const std::size_t row_size_ = terminals_.size() + nt_enums_.size();
        const std::size_t size_ = dfa_.size();
        typename dfa::const_iterator end_ = dfa_.end();

        rules_.symbols(symbols_);
        sm_._columns = terminals_.size() + nt_enums_.size();
        sm_._rows = dfa_.size();
        sm_._table.resize(sm_._columns * sm_._rows);

        for (std::size_t index_ = 0; index_ < size_; ++index_)
        {
            // shift and gotos
            for (typename dfa_state::transition_map::const_iterator iter_ =
                dfa_[index_]._transitions.begin(),
                end_ = dfa_[index_]._transitions.end();
                iter_ != end_; ++iter_)
            {
                const std::size_t id_ = iter_->first;
                typename state_machine::entry &lhs_ =
                    sm_._table[index_ * row_size_ + id_];
                typename state_machine::entry rhs_;

                if (id_ < terminals_.size())
                {
                    // TERMINAL
                    rhs_._action = shift;
                }
                else
                {
                    // NON_TERMINAL
                    rhs_._action = go_to;
                }

                rhs_._param = iter_->second;
                fill_entry(rules_, configs_[index_], symbols_,
                    lhs_, id_, rhs_, warnings_);
            }

            // reductions
            for (typename size_t_pair_set::const_iterator iter_ =
                configs_[index_].begin(), end_ = configs_[index_].end();
                iter_ != end_; ++iter_)
            {
                const production &production_ = grammar_[iter_->first];

                if (production_._symbols._rhs.size() == iter_->second)
                {
                    typename nt_state::size_t_set follow_set_
                        (terminals_.size(), 0);

                    // config is reduction
                    for (typename prod_deque::const_iterator p_iter_ =
                        new_grammar_.begin(), p_end_ = new_grammar_.end();
                        p_iter_ != p_end_; ++p_iter_)
                    {
                        if (production_._symbols == p_iter_->_symbols &&
                            index_ == p_iter_->_rhs_indexes.back().second)
                        {
                            ostringstream lhs_;
                            std::size_t lhs_id_ = ~0;

                            lhs_ << production_._symbols._lhs;

                            if (production_._symbols._lhs != start_)
                            {
                                lhs_ << '_' << p_iter_->_lhs_indexes.first <<
                                    '_' << p_iter_->_lhs_indexes.second;
                            }

                            lhs_id_ = new_nt_enums_.find(lhs_.str())->second;

                            typename size_t_nt_state_map::const_iterator
                                nt_iter_ = new_nt_states_.find(lhs_id_);

                            set_union(follow_set_,
                                nt_iter_->second._follow_set);
                        }
                    }

                    for (std::size_t i_ = 0, size_ = follow_set_.size();
                        i_ < size_; ++i_)
                    {
                        if (!follow_set_[i_]) continue;
                        
                        typename state_machine::entry &lhs_ = sm_._table
                            [index_ * row_size_ + i_];
                        typename state_machine::entry rhs_
                            (reduce, production_._index);

                        if (production_._symbols._lhs == start_)
                        {
                            rhs_._action = accept;
                        }

                        fill_entry(rules_, configs_[index_], symbols_,
                            lhs_, i_, rhs_, warnings_);
                    }
                }
            }
        }
    }

    // Add every element of rhs_ to lhs_. Return true if lhs_ changes.
    static bool set_union(std::vector<char> &lhs_,
        const std::vector<char> &rhs_)
    {
        const std::size_t size_ = lhs_.size();
        bool progress_ = false;
        char *s1_ = &lhs_.front();
        const char *s2_ = &rhs_.front();

        for (std::size_t i_ = 0; i_ < size_; i_++)
        {
            if (s2_[i_] == 0) continue;

            if (s1_[i_] == 0)
            {
                s1_[i_] = 1;
                progress_ = true;
            }
        }

        return progress_;
    }

    static void fill_entry(const rules &rules_, const size_t_pair_set &config_,
        const symbol_map &symbols_, typename state_machine::entry &lhs_,
        const std::size_t id_, const typename state_machine::entry &rhs_,
        std::string *warnings_)
    {
        const grammar &grammar_ = rules_.grammar();
        const terminal_map &terminals_ = rules_.terminals();
        static const char *actions_ [] =
            { "ERROR", "SHIFT", "REDUCE", "GOTO", "ACCEPT" };
        bool error_ = false;

        if (lhs_._action == error)
        {
            if (lhs_._param == syntax_error)
            {
                // No conflict
                lhs_ = rhs_;
            }
            else
            {
                error_ = true;
            }
        }
        else
        {
            std::size_t lhs_prec_ = 0;
            typename token_info::associativity lhs_assoc_ =
                token_info::token;
            std::size_t rhs_prec_ = 0;
            typename token_info::associativity rhs_assoc_ =
                token_info::token;
            typename terminal_map::const_iterator iter_ =
                terminals_.find(symbols_.find(id_)->second);

            if (lhs_._action == shift)
            {
                lhs_prec_ = iter_->second._precedence;
                lhs_assoc_ = iter_->second._associativity;
            }
            else if (lhs_._action == reduce)
            {
                lhs_prec_ = grammar_[lhs_._param]._precedence;
            }

            if (rhs_._action == shift)
            {
                rhs_prec_ = iter_->second._precedence;
                rhs_assoc_ = iter_->second._associativity;
            }
            else if (rhs_._action == reduce)
            {
                rhs_prec_ = grammar_[rhs_._param]._precedence;
            }

            if (lhs_._action == shift && rhs_._action == reduce)
            {
                if (lhs_prec_ == 0 || rhs_prec_ == 0)
                {
                    // Favour shift (leave rhs as it is).
                    if (warnings_)
                    {
                        std::ostringstream ss_;

                        ss_ << actions_[lhs_._action];
                        dump_action(grammar_, config_, symbols_,
                            id_, lhs_, ss_);
                        ss_ << '/' << actions_[rhs_._action];
                        dump_action(grammar_, config_, symbols_,
                            id_, rhs_, ss_);
                        ss_ << " conflict.\n";
                        *warnings_ += ss_.str();
                    }
                }
                else if (lhs_prec_ == rhs_prec_)
                {
                    switch (lhs_assoc_)
                    {
                        case token_info::precedence:
                            // Favour shift (leave rhs as it is).
                            if (warnings_)
                            {
                                std::ostringstream ss_;

                                ss_ << actions_[lhs_._action];
                                dump_action(grammar_, config_, symbols_,
                                    id_, lhs_, ss_);
                                ss_ << '/' << actions_[rhs_._action];
                                dump_action(grammar_, config_, symbols_,
                                    id_, rhs_, ss_);
                                ss_ << " conflict.\n";
                                *warnings_ += ss_.str();
                            }

                            break;
                        case token_info::nonassoc:
                            lhs_._action = error;
                            lhs_._param = non_associative;
                            break;
                        case token_info::left:
                            lhs_ = rhs_;
                            break;
                    }
                }
                else if (rhs_prec_ > lhs_prec_)
                {
                    lhs_ = rhs_;
                }
            }
            else if (lhs_._action == reduce && rhs_._action == reduce)
            {
                if (lhs_prec_ == 0 || rhs_prec_ == 0 || lhs_prec_ == rhs_prec_)
                {
                    error_ = true;
                }
                else if (rhs_prec_ > lhs_prec_)
                {
                    lhs_ = rhs_;
                }
            }
            else
            {
                error_ = true;
            }
        }

        if (error_ && warnings_)
        {
            std::ostringstream ss_;

            ss_ << actions_[lhs_._action];
            dump_action(grammar_, config_, symbols_, id_, lhs_, ss_);
            ss_ << '/' << actions_[rhs_._action];
            dump_action(grammar_, config_, symbols_, id_, rhs_, ss_);
            ss_ << " conflict.\n";
            *warnings_ += ss_.str();
            //throw runtime_error(ss_.str());
        }
    }

    static void dump_action(const grammar &grammar_,
        const size_t_pair_set &config_, const symbol_map &symbols_,
        const std::size_t id_, const typename state_machine::entry &entry_,
        std::ostringstream &ss_)
    {
        if (entry_._action == shift)
        {
            typename size_t_pair_set::const_iterator iter_ = config_.begin();
            typename size_t_pair_set::const_iterator end_ = config_.end();

            for (; iter_ != end_; ++iter_)
            {
                const production &production_ = grammar_[iter_->first];

                if (production_._symbols._rhs.size() > iter_->second &&
                    production_._symbols._rhs[iter_->second]._name ==
                    symbols_.find(id_)->second)
                {
                    dump_production(production_, iter_->second, ss_);
                }
            }
        }
        else if (entry_._action == reduce)
        {
            const production &production_ = grammar_[entry_._param];

            dump_production(production_, ~0, ss_);
        }
    }

    static void dump_production(const production &production_,
        const std::size_t dot_, std::ostringstream &ss_)
    {
        typename symbol_deque::const_iterator sym_iter_ =
            production_._symbols._rhs.begin();
        typename symbol_deque::const_iterator sym_end_ =
            production_._symbols._rhs.end();
        std::size_t index_ = 0;

        ss_ << " (";
        narrow(production_._symbols._lhs.c_str(), ss_);
        ss_ << " -> ";

        if (sym_iter_ != sym_end_)
        {
            if (index_ == dot_) ss_ << ". ";

            narrow(sym_iter_++->_name.c_str(), ss_);
            ++index_;
        }

        for (; sym_iter_ != sym_end_; ++sym_iter_, ++index_)
        {
            ss_ << ' ';

            if (index_ == dot_) ss_ << ". ";

            narrow(sym_iter_->_name.c_str(), ss_);
        }

        ss_ << ')';
    }

    static void copy_rules(const rules &rules_, const nt_enum_map &nt_enums_,
        state_machine &sm_)
    {
        const grammar &grammar_ = rules_.grammar();
        const terminal_map &terminals_ = rules_.terminals();
        typename grammar::const_iterator iter_ = grammar_.begin();
        typename grammar::const_iterator end_ = grammar_.end();

        for (; iter_ != end_; ++iter_)
        {
            const production &production_ = *iter_;
            typename symbol_deque::const_iterator rhs_iter_ =
                production_._symbols._rhs.begin();
            typename symbol_deque::const_iterator rhs_end_ =
                production_._symbols._rhs.end();

            sm_._rules.push_back(typename state_machine::size_t_size_t_pair());

            typename state_machine::size_t_size_t_pair &pair_ =
                sm_._rules.back();

            pair_.first = nt_enums_.find(production_._symbols._lhs)->second;

            for (; rhs_iter_ != rhs_end_; ++rhs_iter_)
            {
                const symbol &symbol_ = *rhs_iter_;

                if (symbol_._type == symbol::TERMINAL)
                {
                    pair_.second.push_back(terminals_.find
                        (symbol_._name)->second._id);
                }
                else
                {
                    pair_.second.push_back(nt_enums_.find
                        (symbol_._name)->second);
                }
            }
        }
    }

    static std::size_t npos()
    {
        return ~0;
    }
};

typedef basic_generator<rules> generator;
typedef basic_generator<wrules> wgenerator;
}

#endif
