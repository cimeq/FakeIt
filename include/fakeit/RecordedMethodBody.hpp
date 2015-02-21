/*
 * Copyright (c) 2014 Eran Pe'er.
 *
 * This program is made available under the terms of the MIT License.
 *
 * Created on Mar 10, 2014
 */

#ifndef RecordedMethodBody_h__
#define RecordedMethodBody_h__

#include <vector>
#include <functional>
#include <tuple>

#include "mockutils/TupleDispatcher.hpp"
#include "fakeit/DomainObjects.hpp"
#include "fakeit/ActualInvocation.hpp"
#include "fakeit/invocation_matchers.hpp"
#include "fakeit/FakeitEvents.hpp"
#include "fakeit/FakeitExceptions.hpp"
#include "mockutils/MethodInvocationHandler.hpp"

namespace fakeit {

/**
 * A composite MethodInvocationHandler that holds a list of ActionSequence objects.
 */
template<typename C, typename R, typename ... arglist>
class RecordedMethodBody: public virtual MethodInvocationHandler<R, arglist...>, public virtual ActualInvocationsSource {

	struct MatchedInvocationHandler: public MethodInvocationHandler<R, arglist...> {

		virtual ~MatchedInvocationHandler() = default;

		MatchedInvocationHandler(typename ActualInvocation<arglist...>::Matcher* matcher,
				MethodInvocationHandler<R, arglist...>* invocationHandler) :
				_matcher { matcher }, _invocationHandler { invocationHandler } {
		}

		R handleMethodInvocation(arglist&... args) override {
			Destructable & destructable = *_invocationHandler;
			MethodInvocationHandler<R, arglist...>& invocationHandler = dynamic_cast<MethodInvocationHandler<R, arglist...>&>(destructable);
			return invocationHandler.handleMethodInvocation(args...);
		}

		typename ActualInvocation<arglist...>::Matcher& getMatcher() const {
			Destructable & destructable = *_matcher;
			typename ActualInvocation<arglist...>::Matcher& matcher = dynamic_cast<typename ActualInvocation<arglist...>::Matcher&>(destructable);
			return matcher;
		}

	private:
		std::shared_ptr<Destructable> _matcher;
		std::shared_ptr<Destructable> _invocationHandler;
	};


	MockObject<C>& _mock;
	MethodInfo _method;

	std::vector<std::shared_ptr<Destructable>>_invocationHandlers;
	std::vector<std::shared_ptr<Destructable>> _actualInvocations;

	MatchedInvocationHandler* buildMatchedInvocationHandler(
			typename ActualInvocation<arglist...>::Matcher* invocationMatcher,
			MethodInvocationHandler<R, arglist...>* invocationHandler) {
		return new MatchedInvocationHandler(invocationMatcher, invocationHandler);
	}

	MatchedInvocationHandler* getInvocationHandlerForActualArgs(ActualInvocation<arglist...>& invocation) {
		for (auto i = _invocationHandlers.rbegin(); i != _invocationHandlers.rend(); ++i) {
			std::shared_ptr<Destructable> curr = *i;
			Destructable& Destructable = *curr;
			MatchedInvocationHandler& im = asMatchedInvocationHandler(Destructable);
			if (im.getMatcher().matches(invocation)) {
				return &im;
			}
		}
		return nullptr;
	}

	MatchedInvocationHandler& asMatchedInvocationHandler(Destructable& destructable)
	{
		MatchedInvocationHandler& im = dynamic_cast<MatchedInvocationHandler&>(destructable);
		return im;
	}

public:

	RecordedMethodBody(MockObject<C>& mock, std::string name) :
		_mock(mock), _method{ nextMethodOrdinal(), name }
	{}

	virtual ~RecordedMethodBody() {
	}

	MethodInfo & getMethod() {
		return _method;
	}

	bool isOfMethod(MethodInfo & method){
		//return &method == &_method;
		return method.id() == _method.id();
	}

	void addMethodInvocationHandler(typename ActualInvocation<arglist...>::Matcher* matcher,
			MethodInvocationHandler<R, arglist...>* invocationHandler) {
		auto* mock = buildMatchedInvocationHandler(matcher, invocationHandler);
		std::shared_ptr<Destructable> destructable{mock};
		_invocationHandlers.push_back(destructable);
	}

	void clear() {
		_invocationHandlers.clear();
	}


	R handleMethodInvocation(arglist&... args) override {
		int ordinal = nextInvocationOrdinal();
		MethodInfo & method = this->getMethod();
		auto actualInvoaction = new ActualInvocation<arglist...>(ordinal, method, args...);

		// ensure deletion if not added to actual invocations.
		std::shared_ptr<Destructable> actualInvoactionDtor{actualInvoaction};

		auto invocationHandler = getInvocationHandlerForActualArgs(*actualInvoaction);
		if (!invocationHandler) {
			UnexpectedMethodCallEvent event(UnexpectedType::Unmatched, *actualInvoaction);
			_mock.getFakeIt().handle(event);

			std::string format{_mock.getFakeIt().format(event)};
			UnexpectedMethodCallException e(format);
			throw e;
		}

		auto& matcher = invocationHandler->getMatcher();
		actualInvoaction->setActualMatcher(&matcher);
		_actualInvocations.push_back(actualInvoactionDtor);
		try {
			return invocationHandler->handleMethodInvocation(args...);
		} catch (NoMoreRecordedActionException&) {
			UnexpectedMethodCallEvent event(UnexpectedType::Unmatched, *actualInvoaction);
			_mock.getFakeIt().handle(event);

			std::string format{_mock.getFakeIt().format(event)};
			UnexpectedMethodCallException e(format);
			throw e;
		}
	}

	void scanActualInvocations(const std::function<void(ActualInvocation<arglist...>&)>& scanner) {
		for (auto destructablePtr : _actualInvocations) {
			ActualInvocation<arglist...>& invocation = asActualInvocation(*destructablePtr);
			scanner(invocation);
		}
	}

	void getActualInvocations(std::unordered_set<Invocation*>& into) const {
		for (auto destructablePtr : _actualInvocations) {
			ActualInvocation<arglist...>& invocation = asActualInvocation(*destructablePtr);
			into.insert(&invocation);
		}
	}

	ActualInvocation<arglist...>& asActualInvocation(Destructable& destructable) const {
		ActualInvocation<arglist...>& invocation = dynamic_cast<ActualInvocation<arglist...>&>(destructable);
		return invocation;
	}

	void setMethodDetails(const std::string& mockName, const std::string& methodName) {
		const std::string fullName {mockName + "." + methodName};
		_method.setName(fullName);
	}

};

}

#endif // MethodMock_h__
