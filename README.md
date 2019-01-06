# Highload Cup 2018

Rating: https://highloadcup.ru/ru/rating/

Rules: https://highloadcup.ru/ru/round/4/

## Overview

### Assumptions
- Names only contain cyrillic symbols
- Time is static

### 0. Imaginary database schema
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

### 1. Filter
Implemented as set intersections and unions with help from binsearch

```
Sex -> Set Id
Domain -> Set Id
Set (Email -> Set Id)
Status -> Set Id
Fname -> Set Id
Set (Sname -> Set Id) -- maybe a prefix tree
PhoneCode -> Set Id
PhonePresent -> Set Id
Country -> Set Id
City -> Set Id
Set Id ordered by birth
Interest -> Set Id
Id -> Set Id -- liked by
PremiumNow -> Set Id
PremiumPresent -> SetId

Id -> Account
```

### 2. Group
Naive combinatorial enumeration, then stable_partition + stable_sort

### 3. Recommend
nth_element + sort

### 4. Suggest
Naive combinatorial enumeration, then stable_partition + stable_sort

```
Id -> Set Like where Like = { int other; int net_timestamp; int num_timestamps; } ordered by 'other'
```