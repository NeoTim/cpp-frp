#ifndef _FRP_PUSH_REPOSITORY_H_
#define _FRP_PUSH_REPOSITORY_H_

#include <frp/execute_on.h>
#include <frp/util/function.h>
#include <frp/util/observable.h>
#include <frp/util/observe_all.h>
#include <frp/util/reference.h>
#include <frp/util/storage.h>
#include <frp/util/variadic.h>
#include <frp/util/vector.h>

namespace frp {
namespace push {
namespace impl {

template<typename Commit, typename Comparator, typename Revisions>
void submit_commit(const std::shared_ptr<std::shared_ptr<Commit>> &previous,
		const std::shared_ptr<util::observable_type> &observable,
		const Comparator &comparator, const Revisions &revisions,
		const std::shared_ptr<Commit> &commit) {
	auto value(std::atomic_load(&*previous));
	bool exchanged(false);
	do {
		commit->revision = (value ? value->revision : util::default_revision) + 1;
	} while ((!value || (value->is_newer(revisions) && !commit->compare_value(*value, comparator)))
		&& !(exchanged = std::atomic_compare_exchange_strong(&*previous, &value, commit)));
	if (exchanged) {
		observable->update();
	}
}

template<typename Storage, typename Generator, typename Comparator, typename... Ts>
void attempt_commit(const std::shared_ptr<std::shared_ptr<Storage>> &storage,
	const std::shared_ptr<util::observable_type> &observable, Generator &generator,
	Comparator &comparator, const std::shared_ptr<Ts> &... dependencies) {
	if (util::all_true(dependencies...)) {
		typedef std::array<util::revision_type, sizeof...(Ts)> revisions_type;
		revisions_type revisions{ dependencies->revision... };
		auto value(std::atomic_load(&*storage));
		if (!value || value->is_newer(revisions)) {
			generator(std::bind(&submit_commit<Storage, Comparator, revisions_type>, storage,
				observable, comparator, revisions, std::placeholders::_1), value, dependencies...);
		}
	}
}

template<typename Storage, typename Generator, typename Comparator, typename... Dependencies>
void attempt_commit_callback(const std::weak_ptr<std::shared_ptr<Storage>> weak_storage,
		Generator &generator, Comparator &comparator,
		const std::shared_ptr<util::observable_type> &observable,
		const std::shared_ptr<std::tuple<Dependencies...>> &dependencies) {
	auto storage(weak_storage.lock());
	if (storage) {
		util::invoke([&](Dependencies&... dependencies) {
			attempt_commit(storage, observable, generator, comparator,
				util::unwrap_reference(dependencies).get()...);
		}, std::ref(*dependencies));
	}
}

template<typename T, typename Storage, typename Comparator, typename Generator,
	typename... Dependencies>
auto make_repository(Generator &&generator, Dependencies &&... dependencies) {
	auto storage(std::make_shared<std::shared_ptr<Storage>>());
	auto observable(std::make_shared<util::observable_type>());
	auto shared_dependencies(std::make_shared<std::tuple<Dependencies...>>(
		std::forward<Dependencies>(dependencies)...));
	auto callback(std::bind(&attempt_commit_callback<Storage, Generator, Comparator, Dependencies...>,
		std::weak_ptr<std::shared_ptr<Storage>>(storage), std::forward<Generator>(generator),
		Comparator(), observable, shared_dependencies));
	auto provider([=]() { return std::atomic_load(&*storage); });
	repository_type<T> repository(observable, callback, provider, shared_dependencies);
	callback();
	return repository;
}

} // namespace impl

template<typename T>
struct repository_type {

	typedef T value_type;

	repository_type() = default;
	repository_type(const repository_type &) = delete;
	repository_type(repository_type &&) = default;
	repository_type &operator=(const repository_type &) = delete;
	repository_type &operator=(repository_type &&) = default;

	// TODO(gardell): Make private, all usage must be through make
	template<typename Update, typename Provider, typename... Dependencies>
	explicit repository_type(const std::shared_ptr<util::observable_type> &observable, Update update,
		Provider &&provider, const std::shared_ptr<std::tuple<Dependencies...>> &dependencies)
		: observable(observable)
		, callbacks(util::vector_from_array(util::invoke(
			util::observe_all(std::forward<Update>(update)), dependencies)))
		, provider(std::forward<Provider>(provider)) {}

	auto get() const {
		return provider();
	}

	template<typename F>
	auto add_callback(F &&f) const {
		return observable->add_callback(std::forward<F>(f));
	}

	std::function<std::shared_ptr<util::storage_type<T>>()> provider;
	std::shared_ptr<util::observable_type> observable;
	std::vector<util::observable_type::reference_type> callbacks;
};

} // namespace push
} // namespace frp

#endif  // _FRP_PUSH_REPOSITORY_H_
