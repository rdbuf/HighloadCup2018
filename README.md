# Highload Cup 2018

Rating: https://highloadcup.ru/ru/rating/

Rules: https://highloadcup.ru/ru/round/4/

## Overview

### Roadmap
- [x] Indices
- [x] JSON parsing
- [ ] Request parsing
	- [x] Lexing
	- [ ] Evaluation
- [ ] JSON printing
- [ ] Asio setup
- [ ] Perf setup (inside of Docker)
- [ ] removal, locks, uniqueness

### Assumptions
- Names only contain cyrillic symbols
- Overall, there are only cyrillic and ascii characters present
- Time is static
- Ids are mostly contiguous
- There are no collisions of string hashes
- See parsers for the input language specs
- Phone code fits into uint16_t
- Initial data are valid
- Prefix and code of any phone number begin with a non-zero digit, and all the parts fit into their respective bitfields of 8, 16 and 36 bits
- Interests intersection of any two users does not exceed 20
- IDNs are not involved (https://en.wikipedia.org/wiki/Internationalized_domain_name)

### Thanks
Utilities that saved me a lot of time:
- valgrind
- pegtl

### Imaginary database schema
```
CREATE TYPE sex_t AS ENUM ('f', 'm');
CREATE TYPE like_t AS (id INTEGER, ts timestamp);
CREATE TABLE Accounts (
	id INTEGER UNIQUE NOT NULL,
	email VARCHAR(100) UNIQUE NOT NULL,
	fname VARCHAR(50),
	sname VARCHAR(50),
	phone VARCHAR(16) UNIQUE,
	sex sex_t NOT NULL,
	birth timestamp NOT NULL,
	country VARCHAR(50),
	city VARCHAR(50),
	joined timestamp NOT NULL,
	interests VARCHAR(100)[],
	premium_start timestamp,
	premium_finish timestamp,
	likes like_t[]
);
```

### Requests
#### 1. Filter
Implemented as set intersections and unions with help from binsearch.
1. Collect all participating sets in intersection
2. Sort by size
3. Intersect in this order

```
Sex -> Set Id
Domain -> Set Id
Set Id -- sorted by email
Status -> Set Id
Fname -> Set Id
Set (Sname -> Set Id) -- maybe a prefix tree
PhoneCode -> Set Id
PhonePresent -> Set Id
Country -> Set Id
City -> Set Id
Set Id -- sorted by birth
Interest -> Set Id
Id -> Set Id -- liked by
PremiumNow -> Set Id
PremiumPresent -> SetId

Id -> Account
```

#### 2. Group
Naive combinatorial enumeration, then stable_partition + stable_sort

#### 3. Recommend
nth_element + sort

#### 4. Suggest
Naive combinatorial enumeration, then stable_partition + stable_sort

```
Id -> Set Like where Like = { int other; int net_timestamp; int num_timestamps; } ordered by 'other'
```