/*
 * JunctionTree-inl.h
 *
 *   Created on: Feb 4, 2010
 *       Author: nikai
 *  Description: the junction tree
 */

#pragma once

#include <boost/foreach.hpp>

#include "SymbolicFactorGraph.h"
#include "BayesTree-inl.h"
#include "JunctionTree.h"

namespace gtsam {

	using namespace std;

	/* ************************************************************************* */
	template <class FG>
	bool JunctionTree<FG>::Clique::equals(const JunctionTree<FG>::Clique& other) const {
		if (!frontal_.equals(other.frontal_))
			return false;

		if (!separator_.equals(other.separator_))
			return false;

		if (children_.size() != other.children_.size())
			return false;

		typename vector<shared_ptr>::const_iterator it1 = children_.begin();
		typename vector<shared_ptr>::const_iterator it2 = other.children_.begin();
		for(; it1!=children_.end(); it1++, it2++)
			if (!(*it1)->equals(**it2)) return false;

		return true;
	}

	/* ************************************************************************* */
	/**
	 * JunctionTree
	 */
	template <class FG>
	void JunctionTree<FG>::Clique::print(const string& indent) const {
		// FG::print(indent);
		cout << "kai1" << endl;
		cout << indent;
		BOOST_FOREACH(const Symbol& key, frontal_)
		cout << (string)key << " ";
		cout << ":";
		BOOST_FOREACH(const Symbol& key, separator_)
		cout << (string)key << " ";
		cout << endl;
	}

	/* ************************************************************************* */
	template <class FG>
	void JunctionTree<FG>::Clique::printTree(const string& indent) const {
		print(indent);
		BOOST_FOREACH(const shared_ptr& child, children_)
			child->printTree(indent+"  ");
	}

	/* ************************************************************************* */
	template <class FG>
	JunctionTree<FG>::JunctionTree(FG& fg, const Ordering& ordering) {
		// Symbolic factorization: GaussianFactorGraph -> SymbolicFactorGraph -> SymbolicBayesNet -> SymbolicBayesTree
		SymbolicFactorGraph sfg(fg);
		SymbolicBayesNet sbn = sfg.eliminate(ordering);
		BayesTree<SymbolicConditional> sbt(sbn);
		sbt.print("sbt");

		// distribtue factors
		root_ = distributeFactors(fg, sbt.root());
	}

	/* ************************************************************************* */
	template <class FG>
	typename JunctionTree<FG>::sharedClique JunctionTree<FG>::distributeFactors(FG& fg,
			const BayesTree<SymbolicConditional>::sharedClique bayesClique) {
		// create a new clique in the junction tree
		sharedClique clique(new Clique());
		clique->frontal_ = bayesClique->ordering();
		clique->separator_.insert(bayesClique->separator_.begin(), bayesClique->separator_.end());

		// recursively call the children
		BOOST_FOREACH(const BayesTree<SymbolicConditional>::sharedClique bayesChild, bayesClique->children()) {
			sharedClique child = distributeFactors(fg, bayesChild);
			clique->children_.push_back(child);
			child->parent_ = clique;
		}

		// collect the factors
		typedef vector<typename FG::sharedFactor> Factors;
		BOOST_FOREACH(const Symbol& frontal, clique->frontal_) {
			Factors factors = fg.template findAndRemoveFactors<Factors>(frontal);
			BOOST_FOREACH(const typename FG::sharedFactor& factor_, factors) {
				clique->push_back(factor_);
			}
		}

		return clique;
	}

	/* ************************************************************************* */
	template <class FG> template <class Conditional>
	pair<FG, typename BayesTree<Conditional>::sharedClique>
	JunctionTree<FG>::eliminateOneClique(sharedClique current, BayesTree<Conditional>& bayesTree) {
		typedef typename BayesTree<Conditional>::sharedClique sharedBtreeClique;
		FG fg; // factor graph will be assembled from local factors and marginalized children
		list<sharedBtreeClique> children;
		fg.push_back(*current); // add the local factor graph
		BOOST_FOREACH(sharedClique& child, current->children_) {
			// receive the factors from the child and its clique point
			FG fgChild; sharedBtreeClique cliqueChild;
			boost::tie(fgChild, cliqueChild) = eliminateOneClique(child, bayesTree);
			if (!cliqueChild.get()) throw runtime_error("eliminateOneClique: child clique is invalid!");

			fg.push_back(fgChild);
			children.push_back(cliqueChild);
		}

		// eliminate the combined factors
		// warning: fg is being eliminated in-place and will contain marginal afterwards
//		BayesNet<Conditional> bn = fg.eliminate(current->frontal_);
		BayesNet<Conditional> bn = fg.eliminateFrontals(current->frontal_);

		// create a new clique corresponding the combined factors
		sharedBtreeClique new_clique = bayesTree.insert(bn, children);

		return make_pair(fg, new_clique);
	}

	/* ************************************************************************* */
	template <class FG> template <class Conditional>
	BayesTree<Conditional> JunctionTree<FG>::eliminate() {
		BayesTree<Conditional> bayesTree;
		eliminateOneClique(root_, bayesTree);
		return bayesTree;
	}

	/* ************************************************************************* */
	template <class FG>
	bool JunctionTree<FG>::equals(const JunctionTree<FG>& other, double tol) const {
		if (!root_ || !other.root_) return false;
		return root_->equals(*other.root_);
	}

	/* ************************************************************************* */
	/**
	 * GaussianJunctionTree
	 */
	template <class FG>
	void GaussianJunctionTree<FG>::btreeBackSubstitue(typename BayesTree<GaussianConditional>::sharedClique current, VectorConfig& config) {
		// solve the bayes net in the current node
		typename BayesNet<GaussianConditional>::const_reverse_iterator it = current->rbegin();
		for (; it!=current->rend(); it++) {
			Vector x = (*it)->solve(config); // Solve for that variable
			config.insert((*it)->key(),x);   // store result in partial solution
		}

		// solve the bayes nets in the child nodes
		typedef typename BayesTree<GaussianConditional>::sharedClique sharedBayesClique;
		BOOST_FOREACH(sharedBayesClique child, current->children_) {
			btreeBackSubstitue(child, config);
		}
	}

	/* ************************************************************************* */
	template <class FG>
	VectorConfig GaussianJunctionTree<FG>::optimize() {
		// eliminate from leaves to the root
		typedef JunctionTree<FG> Base;
		BayesTree<GaussianConditional> bayesTree = this->eliminate<GaussianConditional>();

		// back-substitution
		VectorConfig result;
		btreeBackSubstitue(bayesTree.root(), result);
		return result;
	}

} //namespace gtsam
