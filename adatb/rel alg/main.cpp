#include <iostream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <initializer_list>
#include <numeric>
#include <variant>
#include <set>
#include <functional>

namespace utility {

	template<class F>
	struct function_traits : public function_traits<decltype(&F::operator())> {};

	template <class R, class... Args>
	struct function_traits<R(*)(Args...)> {

		typedef R result;

		using args = typename std::tuple<Args...>;

		template <std::size_t I>
		using arg = std::tuple_element_t<I, args>;

		static constexpr std::size_t arg_num = sizeof...(Args);

	};

	template <class F, class R, class... Args>
	struct function_traits<R(F::*)(Args...)> : public function_traits<R(*)(Args...)> {
		typedef F class_t;
	};

	template <class F, class R, class... Args>
	struct function_traits<R(F::*)(Args...) const> : public function_traits<R(F::*)(Args...)> {};

}

namespace db {

	template <class... Attributes>
	using record = std::tuple<Attributes...>;

	template <class... Attributes>
	struct relation : public std::vector<db::record<Attributes...>> {
		
		using record_type = db::record<Attributes...>;
		using std::vector<record_type>::vector;

		bool operator()(const record_type& rec) const {
			return std::find(this->begin(), this->end(), rec) != this->end();
		}
		
	};

	// Implementációt segítő függvények típusok
	namespace details {

		template <std::size_t N, class Domain, class Relation, class Record, class Invokable>
		void query(Domain&& domain, Relation& rel, Record& rec, Invokable&& invokable) {

			if constexpr (N <= 0) {
				if (std::invoke(invokable, rec)) {
					rel.push_back(rec);
				}
			} else {

				for (auto v: domain.template get<N - 1>()) {
					std::get<N - 1>(rec) = v;
					query<N - 1>(domain, rel, rec, std::forward<Invokable>(invokable));
				}

			}

		}

		template <std::size_t N, class Domain, class Record, class Invokable>
		bool any(Domain&& domain, Record& rec, Invokable&& invokable) {

			if constexpr (N <= 0) {
				return std::invoke(invokable, rec);
			}
			else {

				for (auto v : domain.template get<N - 1>()) {
					std::get<N - 1>(rec) = v;
					if (any<N - 1>(domain, rec, std::forward<Invokable>(invokable))) return true;
				}
				return false;

			}
		}

		template <std::size_t N, class Domain, class Record, class Invokable>
		bool all(Domain&& domain, Record& rec, Invokable&& invokable) {

			if constexpr (N <= 0) {
				return std::invoke(invokable, rec);
			}
			else {

				for (auto v : domain.template get<N - 1>()) {
					std::get<N - 1>(rec) = v;
					if (!all<N - 1>(domain, rec, std::forward<Invokable>(invokable))) return false;
				}
				return true;

			}

		}

		template <template <class...> class RecordType, class... Args>
		constexpr auto create_relation_type_helper(RecordType<Args...> arg) -> db::relation<Args...> {
			return db::relation<Args...>{};
		};

		template <class RecordType>
		using relation_type = decltype(create_relation_type_helper(std::declval<RecordType>()));
	}

	template <class Domain, class Invokable>
	auto query(Domain&& domain, Invokable&& invokable) {
		
		using function_type = ::utility::function_traits<Invokable>;
		using result_relation_type = db::details::relation_type<typename function_type::template arg<0>>;
		constexpr auto arity = std::tuple_size<typename function_type::template arg<0>>::value;
		
		result_relation_type result{};
		typename result_relation_type::record_type rec{};
		
		db::details::query<arity>(domain, result, rec, std::forward<Invokable>(invokable));

		return result;
		
	}

	template <class Domain, class Invokable>
	bool any(Domain&& domain, Invokable&& invokable) {

		using function_type = ::utility::function_traits<Invokable>;
		using result_relation_type = db::details::relation_type<typename function_type::template arg<0>>;
		constexpr auto arity = std::tuple_size<typename function_type::template arg<0>>::value;

		typename result_relation_type::record_type rec{};
		return db::details::any<arity>(domain, rec, std::forward<Invokable>(invokable));
	}

	template <class Domain, class Invokable>
	bool all(Domain&& domain, Invokable&& invokable) {

		using function_type = ::utility::function_traits<Invokable>;
		using result_relation_type = db::details::relation_type<typename function_type::template arg<0>>;
		constexpr auto arity = std::tuple_size<typename function_type::template arg<0>>::value;
		
		typename result_relation_type::record_type rec{};
		return db::details::all<arity>(domain, rec, std::forward<Invokable>(invokable));
	}

	class integer_interpretation_domain {
		std::size_t min;
		std::size_t max;
		std::vector<int> domain;
	public:

		integer_interpretation_domain(std::size_t min, std::size_t max) : min{ min }, max{ max }, domain(max) {
			std::iota(domain.begin(), domain.end(), min);
		}

		// Az N. komponens domain-je matematikai értelemben nem különböztetjük meg őket egy halmaz tartozik az összes komponenshez
		template <std::size_t N>
		auto get() {
			return domain;
		}

	};

	class safe_integer_interpretation_domain {
		std::set<int> domain;
	public:

		void add_to_domain(int v) {
			domain.insert(v);
		}

		// Az N. komponens domain-je matematikai értelemben nem különböztetjük meg őket egy halmaz tartozik az összes komponenshez
		template <std::size_t N>
		auto get() {
			return domain;
		}

	};

}

// db::quary(domain, kifejezés) elkezdi kiértékelni a kifejezést a domain-en és visszaadja az eredményhalmazt (akkor kerül bele egy adott érték, ha a kifejezés igaz rá)
// db::any(domain, kifejezés) elkezdi kiértékelni a kifejezést a domain-en és visszatér igazzal ha a változó valamely értéke mellett igaz a kifejezés
// db::all(domain, kifejezés) elkezdi kiértékelni a kifejezést a domain-en és visszatér igazzal ha a változó bármely értéke mellett igaz a kifejezés


// Mivel szemantikát nem lehet az egyes record oszlopokhoz rendelni kódban, ezért a típusokat nevezzük át, hogy utaljunk rá
namespace datamodel {

	using date = int;
	using sum = int;

}

// Használat:
// Ha R konstant reláció, t sorváltozó:
// sorkalkulus kifejezés -> db::quary
// R(t) -> R(t)
// t[i] -> std::get<i-1>(t)
// műveletek, aritmetikai relációk nem változtak
// egzisztencia kvantor -> db::any
// univerális kvantor -> db::all

int main() {

	// Első közelítésben a [](){} egy syntaxis, amivel functort tudsz írni szebben 
	// Az & jel szükséges, hogy elérjük a lambdán kívüli változókat (referenciaként)
	// Ha többet szeretnél tudni lsd. https://en.cppreference.com/w/cpp/language/lambda
	// Az auto kulcsszó kitalálja a típust, ha az ismert

	// BEVETEL(DATUM, OSSZEG)
	const db::relation<datamodel::date, datamodel::sum> bevetel {
		{ 1, 5},
		{ 2, 6}, 
		{ 3, 7},
		{ 4, 8}
	};

	// BEFIZET(OSSZEG, FIZETETT)
	const db::relation<datamodel::sum, datamodel::sum> befizet {
		{ 5, 1 },
		{ 6, 2 },
		{ 3, 3 },
		{ 2, 4 }
	};

	// Előadáson "A"-val jelöltétek (interpretációs alaphalmaz)
	db::integer_interpretation_domain domain{0, 100}; 
	
	// A kiértékelés bejárjuk a rekord minden komponensével az interpretációs alaphalmazt (ebben az esetben 2 for ciklus, mivel 2 komponense van)
	// A sorkalkulus joinolja a táblákat
	const auto result = db::query(domain, [&](const db::record<int, int, int> t)  {
		return bevetel(std::tuple{ std::get<0>(t), std::get<1>(t) }) && db::any(domain, [&](const db::record<int, int> u) {
			return std::get<1>(t) == std::get<0>(u) && std::get<2>(t) == std::get<1>(u) && befizet(u); 
		});
	});

	for (auto r : result) {
		std::cout << std::get<0>(r) << ' ' << std::get<1>(r) << ' ' << std::get<2>(r) << '\n';
	}

	system("pause");

	return 0;

}
