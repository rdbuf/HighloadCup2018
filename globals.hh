#pragma once

extern std::unordered_map<CoolHash, name_t> fnames;
extern std::unordered_map<CoolHash, name_t> snames;
extern std::unordered_map<CoolHash, country_t> countries;
extern std::unordered_map<CoolHash, city_t> cities;
extern std::unordered_map<CoolHash, interest_t> interests;

extern std::shared_mutex fnames_mutex;
extern std::shared_mutex snames_mutex;
extern std::shared_mutex countries_mutex;
extern std::shared_mutex cities_mutex;
extern std::shared_mutex interests_mutex;