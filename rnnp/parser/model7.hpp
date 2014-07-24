// -*- mode: c++ -*-
//
//  Copyright(C) 2014 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __RNNP__PARSER__MODEL7__HPP__
#define __RNNP__PARSER__MODEL7__HPP__ 1

#include <rnnp/parser/parser.hpp>

namespace rnnp
{
  namespace parser
  {
    struct Model7 : public rnnp::parser::Parser
    {
      template <typename Parser, typename Theta>
      void operation_shift(Parser& parser,
			   const feature_set_type& feats,
			   const Theta& theta,
			   const state_type& state,
			   const word_type& head,
			   const symbol_type& label)
      {
	const size_type offset1 = 0;
	const size_type offset2 = theta.hidden_;
	const size_type offset3 = theta.hidden_ + theta.embedding_;
	const size_type offset4 = theta.hidden_ + theta.embedding_ + theta.hidden_;
	
	state_type state_new = parser.state_allocator_.allocate();
	
	state_new.step()  = state.step() + 1;
	state_new.next()  = state.next() + 1;
	state_new.unary() = state.unary();
      
	state_new.operation() = operation_type::SHIFT;
	state_new.label()     = label;
	state_new.head()      = head;
	state_new.span()      = span_type(state.next(), state.next() + 1);
      
	state_new.stack()      = state;
	state_new.derivation() = state;
	state_new.reduced()    = state_type();
	
	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 state_new.label(),
						 state_new.head(),
						 *state_new.feature_vector());
      
	const size_type offset_classification = theta.offset_classification(label);
	const size_type offset_category       = theta.offset_category(label);

	state_new.layer(theta.hidden_) = (theta.Bsh_.block(offset_category, 0, theta.hidden_, 1)
					  + (theta.Wsh_.block(offset_category, offset1, theta.hidden_, theta.hidden_)
					     * state.layer(theta.hidden_))
					  + (theta.Wsh_.block(offset_category, offset2, theta.hidden_, theta.embedding_)
					     * theta.terminal_.col(theta.terminal(head)))
					  + (theta.Wsh_.block(offset_category, offset3, theta.hidden_, theta.hidden_)
					     * parser.queue_.col(state.next()))
					  + (theta.Wsh_.block(offset_category, offset4, theta.hidden_, theta.hidden_)
					     * parser.buffer_.col(state.next()))
					  ).array().unaryExpr(model_type::activation());
	
	const double score = (theta.Wc_.block(offset_classification, 0, 1, theta.hidden_) * state_new.layer(theta.hidden_))(0, 0);
	
	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector()) + state.score() + score;
      
	parser.agenda_[state_new.step()].push_back(state_new);
      }

      template <typename Parser, typename Theta>
      void operation_reduce(Parser& parser,
			    const feature_set_type& feats,
			    const Theta& theta,
			    const state_type& state,
			    const symbol_type& label)
      {
	const size_type offset1 = 0;
	const size_type offset2 = theta.hidden_;
	const size_type offset3 = theta.hidden_ + theta.hidden_;
	const size_type offset4 = theta.hidden_ + theta.hidden_ + theta.hidden_;
	const size_type offset5 = theta.hidden_ + theta.hidden_ + theta.hidden_ + theta.hidden_;
      
	const state_type state_reduced = state.stack();
	const state_type state_stack   = state_reduced.stack();

	state_type state_new = parser.state_allocator_.allocate();
	  
	state_new.step()  = state.step() + 1;
	state_new.next()  = state.next();
	state_new.unary() = state.unary();
	  
	state_new.operation() = operation_type::REDUCE;
	state_new.label()     = label;
	state_new.head()      = symbol_type::EPSILON;
	state_new.span()      = span_type(state_reduced.span().first_, state.span().last_);
	  
	state_new.stack()      = state_stack;
	state_new.derivation() = state;
	state_new.reduced()    = state_reduced;
	
	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 state_new.label(),
						 state.feature_state(),
						 state_reduced.feature_state(),
						 *state_new.feature_vector());
	  
	const size_type offset_classification = theta.offset_classification(label);
	const size_type offset_category       = theta.offset_category(label);
	  
	state_new.layer(theta.hidden_) = (theta.Bre_.block(offset_category, 0, theta.hidden_, 1)
					  + (theta.Wre_.block(offset_category, offset1, theta.hidden_, theta.hidden_)
					     * state.layer(theta.hidden_))
					  + (theta.Wre_.block(offset_category, offset2, theta.hidden_, theta.hidden_)
					     * state_reduced.layer(theta.hidden_))
					  + (theta.Wre_.block(offset_category, offset3, theta.hidden_, theta.hidden_)
					     * state_stack.layer(theta.hidden_))
					  + (theta.Wre_.block(offset_category, offset4, theta.hidden_, theta.hidden_)
					     * parser.queue_.col(state.next()))
					  + (theta.Wre_.block(offset_category, offset5, theta.hidden_, theta.hidden_)
					     * parser.buffer_.col(state_reduced.span().first_))
					  ).array().unaryExpr(model_type::activation());
	  
	const double score = (theta.Wc_.block(offset_classification, 0, 1, theta.hidden_) * state_new.layer(theta.hidden_))(0, 0);
	  
	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector()) + state.score() + score;
	  
	parser.agenda_[state_new.step()].push_back(state_new);
      }
    
      template <typename Parser, typename Theta>
      void operation_unary(Parser& parser,
			   const feature_set_type& feats,
			   const Theta& theta,
			   const state_type& state,
			   const symbol_type& label)
      {
	const size_type offset1 = 0;
	const size_type offset2 = theta.hidden_;
	const size_type offset3 = theta.hidden_ + theta.hidden_;
	const size_type offset4 = theta.hidden_ + theta.hidden_ + theta.hidden_;

	state_type state_new = parser.state_allocator_.allocate();

	state_new.step()  = state.step() + 1;
	state_new.next()  = state.next();
	state_new.unary() = state.unary() + 1;
      
	state_new.operation() = operation_type(operation_type::UNARY, state.operation().closure() + 1);
	state_new.label()     = label;
	state_new.head()      = symbol_type::EPSILON;
	state_new.span()      = state.span();
      
	state_new.stack()      = state.stack();
	state_new.derivation() = state;
	state_new.reduced()    = state_type();
	
	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 state_new.label(),
						 state.feature_state(),
						 *state_new.feature_vector());
      
	const size_type offset_classification = theta.offset_classification(label);
	const size_type offset_category       = theta.offset_category(label);
      
	state_new.layer(theta.hidden_) = (theta.Bu_.block(offset_category, 0, theta.hidden_, 1)
					  + (theta.Wu_.block(offset_category, offset1, theta.hidden_, theta.hidden_)
					     * state.layer(theta.hidden_))
					  + (theta.Wu_.block(offset_category, offset2, theta.hidden_, theta.hidden_)
					     * state.stack().layer(theta.hidden_))
					  + (theta.Wu_.block(offset_category, offset3, theta.hidden_, theta.hidden_)
					     * parser.queue_.col(state.next()))
					  + (theta.Wu_.block(offset_category, offset4, theta.hidden_, theta.hidden_)
					     * parser.buffer_.col(state.span().first_))
					  ).array().unaryExpr(model_type::activation());
	
	const double score = (theta.Wc_.block(offset_classification, 0, 1, theta.hidden_) * state_new.layer(theta.hidden_))(0, 0);
	
	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector()) + state.score() + score;
	
	parser.agenda_[state_new.step()].push_back(state_new);
      }
      
      template <typename Parser, typename Theta>
      void operation_final(Parser& parser,
			   const feature_set_type& feats,
			   const Theta& theta,
			   const state_type& state)
      {
	state_type state_new = parser.state_allocator_.allocate();

	state_new.step()  = state.step() + 1;
	state_new.next()  = state.next();
	state_new.unary() = state.unary();
      
	state_new.operation() = operation_type::FINAL;
	state_new.label()     = symbol_type::FINAL;
	state_new.head()      = symbol_type::EPSILON;
	state_new.span()      = state.span();
      
	state_new.stack()      = state.stack();
	state_new.derivation() = state;
	state_new.reduced()    = state_type();
	
	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 state.feature_state(),
						 *state_new.feature_vector());

	const size_type offset_classification = theta.offset_classification(symbol_type::FINAL);
      
	state_new.layer(theta.hidden_) = (theta.Bf_
					  + theta.Wf_ * state.layer(theta.hidden_)
					  ).array().unaryExpr(model_type::activation());
      
	const double score = (theta.Wc_.block(offset_classification, 0, 1, theta.hidden_) * state_new.layer(theta.hidden_))(0, 0);
      
	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector()) + state.score() + score;
      
	parser.agenda_[state_new.step()].push_back(state_new);
      }
      
      template <typename Parser, typename Theta>
      void operation_idle(Parser& parser,
			  const feature_set_type& feats,
			  const Theta& theta,
			  const state_type& state)
      {
	state_type state_new = parser.state_allocator_.allocate();

	state_new.step()  = state.step() + 1;
	state_new.next()  = state.next();
	state_new.unary() = state.unary();
      
	state_new.operation() = operation_type::IDLE;
	state_new.label()     = symbol_type::IDLE;
	state_new.head()      = symbol_type::EPSILON;
	state_new.span()      = state.span();
      
	state_new.stack()      = state.stack();
	state_new.derivation() = state;
	state_new.reduced()    = state_type();
	
	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 state.feature_state(),
						 *state_new.feature_vector());

	const size_type offset_classification = theta.offset_classification(symbol_type::IDLE);
      
	state_new.layer(theta.hidden_) = (theta.Bi_
					  + theta.Wi_ * state.layer(theta.hidden_)
					  ).array().unaryExpr(model_type::activation());
      
	const double score = (theta.Wc_.block(offset_classification, 0, 1, theta.hidden_) * state_new.layer(theta.hidden_))(0, 0);
      
	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector()) + state.score() + score;
      
	parser.agenda_[state_new.step()].push_back(state_new);
      }
      
      template <typename Parser, typename Theta>
      void operation_axiom(Parser& parser, 
			   const sentence_type& input,
			   const feature_set_type& feats,
			   const Theta& theta)
      {
	const size_type offset1 = 0;
	const size_type offset2 = theta.hidden_;
	
	const size_type input_size = input.size();
	
	parser.queue_.resize(theta.hidden_, input_size + 1);
	
	parser.queue_.col(input_size) = theta.Bqe_.array().unaryExpr(model_type::activation());
	for (size_type i = input_size; i; -- i)
	  parser.queue_.col(i - 1) = (theta.Bqu_
				      + (theta.Wqu_.block(0, offset1, theta.hidden_, theta.hidden_)
					 * parser.queue_.col(i))
				      + (theta.Wqu_.block(0, offset2, theta.hidden_, theta.embedding_)
					 * theta.terminal_.col(theta.terminal(input[i - 1])))
				      ).array().unaryExpr(model_type::activation());
	
	parser.buffer_.resize(theta.hidden_, input_size + 1);
	
	parser.buffer_.col(0) = theta.Bbs_.array().unaryExpr(model_type::activation());
	for (size_type i = 0; i != input_size; ++ i)
	  parser.buffer_.col(i + 1) = (theta.Bbu_
				       + (theta.Wbu_.block(0, offset1, theta.hidden_, theta.hidden_)
					  * parser.buffer_.col(i))
				       + (theta.Wbu_.block(0, offset2, theta.hidden_, theta.embedding_)
					  * theta.terminal_.col(theta.terminal(input[i])))
				       ).array().unaryExpr(model_type::activation());
	
	state_type state_new = parser.state_allocator_.allocate();

	state_new.step()  = 0;
	state_new.next()  = 0;
	state_new.unary() = 0;
      
	state_new.operation() = operation_type::AXIOM;
	state_new.label()     = symbol_type::AXIOM;
	state_new.head()      = symbol_type::EPSILON;
	state_new.span()      = span_type(-1, 0);
      
	state_new.stack()      = state_type();
	state_new.derivation() = state_type();
	state_new.reduced()    = state_type();

	state_new.feature_vector() = parser.feature_vector_allocator_.allocate();
	state_new.feature_state()  = feats.apply(state_new.operation(),
						 *state_new.feature_vector());

	state_new.score() = rnnp::dot_product(theta.Wfe_, *state_new.feature_vector());
	state_new.layer(theta.hidden_) = theta.Ba_.array().unaryExpr(model_type::activation());
      
	parser.agenda_[state_new.step()].push_back(state_new);
      }
    };
  };
};

#endif