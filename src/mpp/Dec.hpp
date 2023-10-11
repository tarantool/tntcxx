#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <cassert>
#include <cstdint>
#include <utility>

#include "Constants.hpp"
#include "Rules.hpp"
#include "Spec.hpp"

namespace mpp {

using std::integral_constant;
using tnt::CStr;
#ifndef TNT_DISABLE_STR_LITERAL
namespace literal = tnt::literal;
#endif

namespace decode_details {

template <compact::Family ...sequence>
using family_sequence = std::integer_sequence<std::underlying_type_t<compact::Family>, sequence...>;

template <class T>
constexpr auto detectFamily()
{
	using CRU = decltype(unwrap(std::declval<const T&>()));
	using U = std::remove_cv_t<std::remove_reference_t<CRU>>;
	if constexpr (is_wrapped_family_v<T>) {
		return std::integer_sequence<compact::Family, T::family>{};
	} else if constexpr (std::is_same_v<U, std::nullptr_t>) {
		return family_sequence<compact::MP_NIL>{};
	} else if constexpr (std::is_same_v<U, bool>) {
		return family_sequence<compact::MP_BOOL>{};
	} else if constexpr (tnt::is_signed_integer_v<U>) {
		return family_sequence<compact::MP_UINT, compact::MP_INT>{};
	} else if constexpr (tnt::is_unsigned_integer_v<U>) {
		return family_sequence<compact::MP_UINT>{};
	} else if constexpr (std::is_same_v<U, float>) {
		return family_sequence<compact::MP_UINT, compact::MP_INT,
				       compact::MP_FLT, compact::MP_DBL>{};
	} else if constexpr (std::is_same_v<U, double>) {
		return family_sequence<compact::MP_UINT, compact::MP_INT,
				       compact::MP_FLT, compact::MP_DBL>{};
	} else if constexpr (tnt::is_contiguous_char_v<U> &&
			     tnt::is_resizable_v<U>) {
		return family_sequence<compact::MP_STR>{};
//	} else if constexpr (tnt::is_const_pairs_iterable_v<U>) {
//		return compact::MP_MAP;
//	} else if constexpr (tnt::is_const_iterable_v<U>) {
//		return compact::MP_ARR;
	} else if constexpr (tnt::is_tuplish_of_pairish_v<U>) {
		return family_sequence<compact::MP_MAP>{};
	} else if constexpr (tnt::is_tuplish_v<U>) {
		return family_sequence<compact::MP_ARR>{};
	} else {
		static_assert(tnt::always_false_v<U>,
			      "Failed to recognise type");
		return family_sequence<>{};
	}
}

constexpr auto is256 = std::make_index_sequence<256>{};

template <class CONT>
bool
decode(CONT& cont);

template <class CONT, class T, class ...MORE>
bool
decode(CONT& cont, T& t, MORE&... more);

template <class RULE, size_t ...I>
constexpr uint8_t lowerRangesSumLength(std::index_sequence<I...>)
{
	return ((RULE::simplex_ranges[I].second -
		 RULE::simplex_ranges[I].first) + ... + 0);
}

struct SubRule {
	compact::Family family;
	bool is_simplex;
	// index in RULE::simplex_ranges or RULE::complex_types.
	uint8_t i;
	// The first and last tags, valid for ftag <= tag <= ltag.
	uint8_t ftag, ltag;
	constexpr bool overlaps(const SubRule& a) const
	{
		return !(ltag < a.ftag || ftag > a.ltag);
	}
};

template <size_t TN>
struct SubRules {
	static constexpr size_t N = TN;
	SubRule arr[N ? N : 1];
	template <size_t M, size_t ...I, size_t ...J>
	constexpr SubRules<M + N> join(SubRules<M> a,
				       std::index_sequence<I...>,
				       std::index_sequence<J...>)
	{
		return {arr[I]..., a.arr[J]...};
	}

	// Check that the subrules don't overlap in terms of tags.
	template <size_t ...I, size_t ...J>
	constexpr bool check(std::index_sequence<I...>,
			     std::index_sequence<J...>) const
	{
		auto checkX = [&](size_t X){ return (true && ... && (X == I || !arr[X].overlaps(arr[I]))); };
		return (checkX(J) && ...);
	}
	constexpr bool check() const
	{
		return check(std::make_index_sequence<N>{}, std::make_index_sequence<N>{});
	}

	// Find index of subrule by tag.
	template <size_t TAG, size_t I = 0>
	constexpr size_t by_tag() const
	{
		if constexpr (I == N)
			return N;
		else
			return TAG >= arr[I].ftag && TAG <= arr[I].ltag ? I :
			       by_tag<TAG, I + 1>();
	}
};

template <size_t X, size_t Y>
constexpr SubRules<X + Y> operator+(SubRules<X> a, SubRules<Y> b)
{
	return a.join(b, std::make_index_sequence<X>{},
		      std::make_index_sequence<Y>{});
}

template <class RULE, size_t I>
constexpr uint8_t simplex_tag_count_v =
	RULE::simplex_ranges[I].second - RULE::simplex_ranges[I].first;

template <class RULE, size_t I>
constexpr uint8_t simplex_tag_first_v =
	RULE::simplex_tag +
	lowerRangesSumLength<RULE>(std::make_index_sequence<I>{});

template <class RULE, size_t I>
constexpr uint8_t simplex_tag_last_v =
	simplex_tag_first_v<RULE, I> + (simplex_tag_count_v<RULE, I> - 1);

template <compact::Family FAMILY, class RULE, size_t ...I>
constexpr auto getSimplexSubRules(std::index_sequence<I...>)
{
	constexpr size_t simplex_count =
		sizeof(RULE::simplex_ranges) / sizeof(RULE::simplex_ranges[0]);
	return SubRules<simplex_count>{{
		{
			FAMILY, true, I,
			simplex_tag_first_v<RULE, I>,
			simplex_tag_last_v<RULE, I>
		} ...
	}};
}

template <compact::Family FAMILY>
constexpr auto getSimplexSubRules()
{
	using Rule_t = RuleByFamily_t<FAMILY>;
	if constexpr (has_simplex_v<Rule_t>) {
		constexpr size_t simplex_count =
			sizeof(Rule_t::simplex_ranges) /
			sizeof(Rule_t::simplex_ranges[0]);
		constexpr auto is = std::make_index_sequence<simplex_count>{};
		return getSimplexSubRules<FAMILY, Rule_t>(is);
	} else {
		return SubRules<0>{};
	}
}

template <compact::Family FAMILY, class RULE, size_t ...I>
constexpr auto getComplexSubRules(std::index_sequence<I...>)
{
	using types = typename RULE::complex_types;
	constexpr size_t complex_count = std::tuple_size_v<types>;
	return SubRules<complex_count>{{
		{
			FAMILY, false, I,
			RULE::complex_tag + I,
			RULE::complex_tag + I
		} ...
	}};
}

template <compact::Family FAMILY>
constexpr auto getComplexSubRules()
{
	using Rule_t = RuleByFamily_t<FAMILY>;
	if constexpr (has_complex_v<Rule_t>) {
		using types = typename Rule_t::complex_types;
		constexpr size_t complex_count = std::tuple_size_v<types>;
		constexpr auto is = std::make_index_sequence<complex_count>{};
		return getComplexSubRules<FAMILY, Rule_t>(is);
	} else {
		return SubRules<0>{};
	}
}

template <compact::Family FAMILY>
constexpr auto getFamilySubRules()
{
	return getSimplexSubRules<FAMILY>() + getComplexSubRules<FAMILY>();
}

template <compact::Family ...FAMILY>
constexpr auto getSubRules(std::integer_sequence<compact::Family, FAMILY...>)
{
	return (getFamilySubRules<FAMILY>() + ...);
}

template <class CONT, class T, size_t ...I>
bool decodeElems(CONT &cont, T &t, std::index_sequence<I...>)
{
	return decode(cont, tnt::get<I>(t)...);
}

template <class RULE, class CONT, class V, class T>
bool finishReading([[maybe_unused]] CONT &cont, [[maybe_unused]] V val, T &t)
{
	static_assert(!RULE::has_ext);
	if constexpr (RULE::has_data) {
		size_t size = val;
		t.resize(size);
		cont.read({t.data(), size});
	} else if constexpr (RULE::family == compact::MP_ARR) {
		size_t size = val;
		if (size != tnt::tuple_size_v<T>)
			return false;
		return decodeElems(cont, t,
				   std::make_index_sequence<tnt::tuple_size_v<T>>{});
	} else if constexpr (RULE::family == compact::MP_MAP) {
		size_t size = val;
		if (size * 2 != tnt::tuple_size_v<T>)
			return false;
		return decodeElems(cont, t,
				   std::make_index_sequence<2 * tnt::tuple_size_v<T>>{});
	} else if constexpr (RULE::family == compact::MP_NIL) {
		return true;
	} else {
		t = static_cast<T>(val);
	}
	return true;
}

template <class RULE, size_t I, class CONT, class T>
bool readSimplexObject(CONT& cont, uint8_t tag, T& t)
{
	assert(tag >= SimplexRange<RULE>::first);
	assert(tag < SimplexRange<RULE>::second);
	constexpr uint8_t tag0 = RULE::simplex_tag;
	constexpr uint8_t lower_ranges_length =
		lowerRangesSumLength<RULE>(std::make_index_sequence<I>{});
	decltype(RULE::simplex_ranges[0].first) val =
		tag - tag0 - lower_ranges_length + RULE::simplex_ranges[I].first;
	return finishReading<RULE>(cont, val, t);
}

template <class RULE, size_t I, class CONT, class T>
bool readComplexObject(CONT& cont, [[maybe_unused]] uint8_t tag, T& t)
{
	assert(tag >= ComplexRange<RULE>::first);
	assert(tag < ComplexRange<RULE>::second);
	using V = std::tuple_element_t<I, typename RULE::complex_types>;
	under_uint_t<V> u;
	cont.read(u);
	V val = bswap<V>(u);
	return finishReading<RULE>(cont, val, t);
}

template <class RULE, bool IS_SIMPLEX, size_t I, class CONT, class T, class ...MORE>
bool readObject(CONT& cont, uint8_t tag, T& t, MORE&... more)
{
	if constexpr (IS_SIMPLEX) {
		if (!readSimplexObject<RULE, I>(cont, tag, t))
			return false;
	} else {
		if (!readComplexObject<RULE, I>(cont, tag, t))
			return false;
	}
	return decode(cont, more...);
}

template <class CONT, class T, class ...MORE>
bool readFail(CONT&, uint8_t, T&, MORE&...)
{
	return false;
}

template <class CONT, class T, class ...MORE>
using jump_t = bool (*)(CONT&, uint8_t, T& t, MORE&...);

template <class CONT, class T, class ...MORE>
using jumps_t = std::array<jump_t<CONT, T, MORE...>, 256>;

template <class CONT, class T, class ...MORE, compact::Family ...FAMILY,
        size_t... I, size_t... K>
constexpr jumps_t<CONT, T, MORE...>
build_jumps(std::integer_sequence<compact::Family, FAMILY...> family_seq,
	    std::index_sequence<I...>, std::index_sequence<K...>)
{
	constexpr auto sub_rules = getSubRules(family_seq);
	static_assert(sub_rules.check());
	constexpr size_t N = sub_rules.N;
	using j_t = jump_t<CONT, T, MORE...>;
	using js_t = jumps_t<CONT, T, MORE...>;
	constexpr j_t def_jump = &readFail<CONT, T, MORE...>;
	constexpr j_t subrule_jumps[sub_rules.N] = {
		&readObject<RuleByFamily_t<sub_rules.arr[I].family>,
			sub_rules.arr[I].is_simplex, sub_rules.arr[I].i,
			CONT, T, MORE...> ...};
	constexpr js_t arr = {
		(sub_rules.template by_tag<K>() == N ? def_jump :
		 subrule_jumps[sub_rules.template by_tag<K>()])...
	};
	return arr;
}

/** Terminal decode. */
template <class CONT>
bool
decode(CONT&)
{
	return true;
}

template <class CONT, class T, class ...MORE>
bool
decode(CONT& cont, T& t, MORE&... more)
{
	constexpr auto family_seq = detectFamily<T>();
	constexpr auto sub_rules = getSubRules(family_seq);
	constexpr auto is = std::make_index_sequence<sub_rules.N>{};

	auto& u = unwrap(t);
	using U = std::remove_reference_t<decltype(u)>;
	uint8_t tag = cont.template read<uint8_t>();

	using jump_t = bool(*)(CONT&, uint8_t, U&, MORE&...);
	static constexpr std::array<jump_t, 256> jumps =
		build_jumps<CONT, U, MORE...>(family_seq, is, is256);
	return jumps[tag](cont, tag, u, more...);
}

} // namespace decode_details

template <class CONT, class... T>
bool
decode(CONT& cont, T&... t)
{
	// TODO: Guard
	bool res = decode_details::decode(cont, t...);
	return res;
}

} // namespace mpp

#include "../Utils/ObjHolder.hpp"

namespace mpp {

struct DefaultErrorHandler {
	void BadMsgpack() {}
	void WrongType(Family /*expected*/, Family /*got*/) {}
	void MaxDepthReached() {}
};

template <class BUFFER>
struct ReaderTemplate : DefaultErrorHandler {
	static constexpr Family VALID_TYPES = MP_ANY;
	template <class T>
	void Value(const typename BUFFER::iterator&, compact::Family, T&&) {}
	typename BUFFER::iterator* StoreEndIterator() { return nullptr; }
};

template <class BUFFER, Family TYPE>
struct SimpleReaderBase : DefaultErrorHandler {
	using BufferIterator_t = typename BUFFER::iterator;
	static constexpr Family VALID_TYPES = TYPE;
	BufferIterator_t* StoreEndIterator() { return nullptr; }
};

template <class BUFFER, Family TYPE, class T>
struct SimpleReader : SimpleReaderBase<BUFFER, TYPE> {
	using BufferIterator_t = typename BUFFER::iterator;
	explicit SimpleReader(T& t) : value(t) {}
	template <class U>
	void Value(const BufferIterator_t&, compact::Family, U&& u)
	{
		// A check may be required here.
		value = u;
	}
	T& value;
};

template <class BUFFER, size_t MAX_SIZE>
struct SimpleStrReader : SimpleReaderBase<BUFFER, MP_STR> {
	using BufferIterator_t = typename BUFFER::iterator;
	SimpleStrReader(char *dst, size_t &size) : m_Dst(dst), m_Size(size) {}
	void Value(BufferIterator_t& itr, compact::Family, const StrValue& v)
	{
		m_Size = v.size;
		size_t read_size = std::min(MAX_SIZE, m_Size);
		BufferIterator_t walker = itr;
		walker += v.offset;
		for (size_t i = 0; i < read_size; i++) {
			m_Dst[i] = *walker;
			++walker;
		}
	}
	char *m_Dst;
	size_t& m_Size;
};

template <class BUFFER, size_t MAX_SIZE, Family TYPE, class T>
struct SimpleArrDataReader : SimpleReaderBase<BUFFER, TYPE> {
	using BufferIterator_t = typename BUFFER::iterator;
	explicit SimpleArrDataReader(T *dst) : m_Dst(dst) {}
	template <class U>
	void Value(const BufferIterator_t&, compact::Family, U&& u)
	{
		if (m_I >= MAX_SIZE)
			return;
		m_Dst[m_I++] = u;
	}
	T *m_Dst;
	size_t m_I = 0;
};

template <class DEC, class BUFFER, size_t MAX_SIZE, Family TYPE, class T>
struct SimpleArrReader : SimpleReaderBase<BUFFER, MP_ARR> {
	using BufferIterator_t = typename BUFFER::iterator;
	SimpleArrReader(DEC& dec, T *dst, size_t &size)
		: m_Dec(dec), m_Dst(dst), m_Size(size) {}
	void Value(const BufferIterator_t&, compact::Family, ArrValue v)
	{
		m_Size = std::min(MAX_SIZE, (size_t)v.size);
		using Reader_t = SimpleArrDataReader<BUFFER, MAX_SIZE, TYPE, T>;
		m_Dec.SetReader(false, Reader_t{m_Dst});
	}

	DEC& m_Dec;
	T *m_Dst;
	size_t& m_Size;
};

template <class BUFFER>
class Dec
{
public:
	using Dec_t = Dec<BUFFER>;
	using Buffer_t = BUFFER;
	using BufferIterator_t = typename BUFFER::iterator;

	static constexpr size_t MAX_DEPTH = 16;
	static constexpr size_t MAX_READER_SIZE = 32;

	using Transition_t = void(Dec_t::*)();
	struct State {
		const Transition_t *transitions;
		BufferIterator_t *storeEndIterator;
		tnt::ObjHolder<MAX_READER_SIZE> objHolder;
	};

private:
	struct Level {
		State state[2];
		size_t countdown;
		size_t stateMask;
	};
	Level m_Levels[MAX_DEPTH];
	Level *m_CurLevel = m_Levels;

	template<class READER, class... ARGS>
	void FillState(State &st, ARGS&&... args);
	void FillSkipState(State &st, BufferIterator_t *save_end = nullptr);

public:
	explicit Dec(Buffer_t &buf)
		: m_Buf(buf), m_Cur(m_Buf.begin())
	{
		for (auto& l : m_Levels)
			l.countdown = l.stateMask = 0;
	}

	template <class T, class... U>
	void SetReader(bool second, U&&... u);
	template <class T>
	void SetReader(bool second, T&& t);
	void Skip(BufferIterator_t *saveEnd = nullptr);
	void SetPosition(BufferIterator_t &itr);
	BufferIterator_t getPosition() { return m_Cur; }

	inline ReadResult_t Read();

	inline void AbortAndSkipRead(ReadResult_t error = READ_ABORTED_BY_USER);
	inline void AbandonDecoder(ReadResult_t error = READ_ABORTED_BY_USER);

	State& CurState() { return m_CurLevel->state[m_CurLevel->countdown & m_CurLevel->stateMask]; }
	State& LastState() { return m_CurLevel->state[(1 ^ m_CurLevel->countdown) & m_CurLevel->stateMask]; }
	State& FirstState() { return m_CurLevel->state[0]; }
	State& SecondState() { return m_CurLevel->state[1]; }

	template <class DEC, class READER, class SEQUENCE>
	friend struct ReaderMap;

private:
	template <class READER>
	void ReadNil();
	template <class READER>
	void ReadBad();
	template <class READER>
	void ReadBool();
	template <class READER, class T>
	void ReadUint();
	template <class READER, class T>
	void ReadInt();
	template <class READER, class T>
	void ReadFlt();
	template <class READER, class T>
	void ReadStr();
	template <class READER>
	void ReadZeroStr();
	template <class READER, class T>
	void ReadBin();
	template <class READER, class T>
	void ReadArr();
	template <class READER, class T>
	void ReadMap();
	template <class READER, class T>
	void ReadExt();
	template <class READER, uint32_t SIZE>
	void ReadFixedExt();

	inline void SkipCommon();


private:
	Buffer_t& m_Buf;
	BufferIterator_t m_Cur;
	bool m_IsDeadStream = false;
	ReadResult_t m_Result = READ_SUCCESS;
};

template <class T>
constexpr size_t header_size = 1 + sizeof(T);
template <>
constexpr size_t header_size<void> = 1;

template <class BUFFER>
template <class READER>
void
Dec<BUFFER>::ReadNil()
{
	assert(m_Cur.template get<uint8_t>() == 0xc0);
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_NIL;
	[[maybe_unused]] constexpr Family type = MP_NIL;
	READER& r = CurState().objHolder.template get<READER>();

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		r.Value(m_Cur, ctype, nullptr);
	}
	++m_Cur;
}

template <class BUFFER>
template <class READER>
void
Dec<BUFFER>::ReadBad()
{
	assert(m_Cur.template get<uint8_t>() == 0xc1);
	AbandonDecoder(READ_BAD_MSGPACK);
}

template <class BUFFER>
template <class READER>
void
Dec<BUFFER>::ReadBool()
{
	assert((m_Cur.template get<uint8_t>() & 0xfe) == 0xc2);
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_BOOL;
	[[maybe_unused]] constexpr Family type = MP_BOOL;
	READER& r = CurState().objHolder.template get<READER>();

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		bool value = m_Cur.template get<uint8_t>() - 0xc2;
		r.Value(m_Cur, ctype, value);
	}
	++m_Cur;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadUint()
{
	if constexpr (std::is_same_v<T, void>) {
		assert(m_Cur.template get<uint8_t>() < 0x80);
	} else {
		assert((m_Cur.template get<uint8_t>() & 0xfc) == 0xcc);
		assert(sizeof(T) ==
			(1u << (m_Cur.template get<uint8_t>() - 0xcc)));
	}
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_UINT;
	[[maybe_unused]] constexpr Family type = MP_UINT;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}
	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		uint64_t value;
		if constexpr (std::is_same_v<T, void>) {
			value = m_Cur.template get<uint8_t>();
		} else {
			BufferIterator_t step = m_Cur;
			++step;
			value = bswap(step.template get<T>());
		}
		r.Value(m_Cur, ctype, value);
	}
	if constexpr (std::is_same_v<T, void>)
		++m_Cur;
	else
		m_Cur += header_size<T>;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadInt()
{
	if constexpr (std::is_same_v<T, void>) {
		assert(m_Cur.template get<uint8_t>() >= 0xe0);
	} else {
		assert((m_Cur.template get<uint8_t>() & 0xfc) == 0xd0);
		assert(sizeof(T) ==
			(1u << (m_Cur.template get<uint8_t>() - 0xd0)));
	}
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_INT;
	[[maybe_unused]] constexpr Family type = MP_INT;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}
	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		int64_t value;
		if constexpr (std::is_same_v<T, void>) {
			value = m_Cur.template get<int8_t>();
		} else {
			BufferIterator_t step = m_Cur;
			++step;
			using U = under_uint_t<T>;
			U u = bswap(step.template get<U>());
			value = static_cast<T>(u);
		}
		r.Value(m_Cur, ctype, value);
	}
	if constexpr (std::is_same_v<T, void>)
		++m_Cur;
	else
		m_Cur += header_size<T>;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadFlt()
{
	assert((m_Cur.template get<uint8_t>() & 0xfe) == 0xca);
	assert(sizeof(T) == (4u << ((m_Cur.template get<uint8_t>())&1)));
	[[maybe_unused]] constexpr compact::Family ctype =
		sizeof(T) == 4 ? compact::MP_FLT : compact::MP_DBL;
	[[maybe_unused]] constexpr Family type = sizeof(T) == 4 ? MP_FLT : MP_DBL;;
	READER& r = CurState().objHolder.template get<READER>();

	if (!m_Buf.has(m_Cur, header_size<T>)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		T value;
		BufferIterator_t step = m_Cur;
		++step;
		under_uint_t<T> x = bswap(step.template get<under_uint_t<T>>());
		memcpy(&value, &x, sizeof(T));
		r.Value(m_Cur, ctype, value);
	}
	m_Cur += header_size<T>;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadStr()
{
	if constexpr (std::is_same_v<T, void>) {
		assert((m_Cur.template get<uint8_t>() & 0xe0) == 0xa0);
	} else {
		assert(m_Cur.template get<uint8_t>() >= 0xd9);
		assert(m_Cur.template get<uint8_t>() <= 0xdb);
		assert(sizeof(T) ==
		       (1 << (m_Cur.template get<uint8_t>() - 0xd9)));
	}
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_STR;
	[[maybe_unused]] constexpr Family type = MP_STR;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}
	uint32_t str_size;
	if constexpr (std::is_same_v<T, void>) {
		str_size = m_Cur.template get<uint8_t>() - 0xa0;
	} else {
		BufferIterator_t step = m_Cur;
		++step;
		str_size = bswap(step.template get<T>());
	}
	if (!m_Buf.has(m_Cur, header_size<T> + str_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		r.Value(m_Cur, ctype, StrValue{header_size<T>, str_size});
	}
	m_Cur += header_size<T> + str_size;
}

template <class BUFFER>
template <class READER>
void
Dec<BUFFER>::ReadZeroStr()
{
	assert((m_Cur.template get<uint8_t>() & 0xe0) == 0xa0);
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_STR;
	[[maybe_unused]] constexpr Family type = MP_STR;
	READER& r = CurState().objHolder.template get<READER>();

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		r.Value(m_Cur, ctype, StrValue{1, 0});
	}
	++m_Cur;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadBin()
{
	assert(m_Cur.template get<uint8_t>() >= 0xc4);
	assert(m_Cur.template get<uint8_t>() <= 0xc6);
	assert(sizeof(T) == (1 << (m_Cur.template get<uint8_t>() - 0xc4)));
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_BIN;
	[[maybe_unused]] constexpr Family type = MP_BIN;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}
	uint32_t bin_size;
	BufferIterator_t step = m_Cur;
	++step;
	bin_size = bswap(step.template get<T>());
	if (!m_Buf.has(m_Cur, header_size<T> + bin_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		r.Value(m_Cur, ctype, BinValue{header_size<T>, bin_size});
	}
	m_Cur += header_size<T> + bin_size;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadArr()
{
	if constexpr (std::is_same_v<T, void>) {
		assert((m_Cur.template get<uint8_t>() & 0xf0) == 0x90);
	} else {
		assert((m_Cur.template get<uint8_t>() & 0xfe) == 0xdc);
		assert(sizeof(T) ==
		       (2 << (m_Cur.template get<uint8_t>() - 0xdc)));
	}
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_ARR;
	[[maybe_unused]] constexpr Family type = MP_ARR;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}

	uint32_t arr_size;
	if constexpr (std::is_same_v<T, void>) {
		arr_size = m_Cur.template get<uint8_t>() - 0x90;
	} else {
		BufferIterator_t step = m_Cur;
		++step;
		arr_size = bswap(step.template get<T>());
	}

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
		m_CurLevel->countdown += arr_size;
	} else {
		if (m_CurLevel == &m_Levels[MAX_DEPTH - 1]) {
			r.MaxDepthReached();
			AbortAndSkipRead(READ_MAX_DEPTH_REACHED);
			m_CurLevel->countdown += arr_size;
		} else {
			++m_CurLevel;
			m_CurLevel->countdown = arr_size;
			m_CurLevel->stateMask = 0;
			r.Value(m_Cur, ctype,
				ArrValue{header_size<T>, arr_size});
		}
	}
	if constexpr (std::is_same_v<T, void>)
		++m_Cur;
	else
		m_Cur += header_size<T>;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadMap()
{
	if constexpr (std::is_same_v<T, void>) {
		assert((m_Cur.template get<uint8_t>() & 0xf0) == 0x80);
	} else {
		assert((m_Cur.template get<uint8_t>() & 0xfe) == 0xde);
		assert(sizeof(T) ==
		       (2 << (m_Cur.template get<uint8_t>() - 0xde)));
	}
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_MAP;
	[[maybe_unused]] constexpr Family type = MP_MAP;
	READER& r = CurState().objHolder.template get<READER>();

	if constexpr (!std::is_same_v<T, void>) {
		if (!m_Buf.has(m_Cur, header_size<T>)) {
			m_Result = m_Result | READ_NEED_MORE;
			return;
		}
	}

	uint32_t map_size;
	if constexpr (std::is_same_v<T, void>) {
		map_size = m_Cur.template get<uint8_t>() - 0x80;
	} else {
		BufferIterator_t step = m_Cur;
		++step;
		map_size = bswap(step.template get<T>());
	}

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
		m_CurLevel->countdown += 2 * size_t(map_size);
	} else {
		if (m_CurLevel == &m_Levels[MAX_DEPTH - 1]) {
			r.MaxDepthReached();
			AbortAndSkipRead(READ_MAX_DEPTH_REACHED);
			m_CurLevel->countdown += 2 * size_t(map_size);
		} else {
			++m_CurLevel;
			m_CurLevel->countdown = 2 * size_t(map_size);
			m_CurLevel->stateMask = 1;
			r.Value(m_Cur, ctype,
				MapValue{header_size<T>, map_size});
		}
	}
	if constexpr (std::is_same_v<T, void>)
		++m_Cur;
	else
		m_Cur += header_size<T>;
}

template <class BUFFER>
template <class READER, class T>
void
Dec<BUFFER>::ReadExt()
{
	assert(m_Cur.template get<uint8_t>() >= 0xc7);
	assert(m_Cur.template get<uint8_t>() <= 0xc9);
	assert(sizeof(T) == (1 << (m_Cur.template get<uint8_t>() - 0xc7)));
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_EXT;
	[[maybe_unused]] constexpr Family type = MP_EXT;
	READER& r = CurState().objHolder.template get<READER>();

	constexpr size_t header_size = 2 + sizeof(T);
	if (!m_Buf.has(m_Cur, header_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	BufferIterator_t step = m_Cur;
	++step;
	uint32_t ext_size = bswap(step.template get<T>());
	if (!m_Buf.has(m_Cur, header_size + ext_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	step += sizeof(T);

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		int8_t ext_type = step.template get<int8_t>();
		r.Value(m_Cur, ctype,
			ExtValue{ext_type, header_size, ext_size});
	}
	m_Cur += header_size + ext_size;
}

template <class BUFFER>
template <class READER, uint32_t SIZE>
void
Dec<BUFFER>::ReadFixedExt()
{
	assert(m_Cur.template get<uint8_t>() >= 0xd4);
	assert(m_Cur.template get<uint8_t>() <= 0xd8);
	assert(SIZE == (1 << (m_Cur.template get<uint8_t>() - 0xd4)));
	[[maybe_unused]] constexpr compact::Family ctype = compact::MP_EXT;
	[[maybe_unused]] constexpr Family type = MP_EXT;
	READER& r = CurState().objHolder.template get<READER>();

	constexpr size_t header_size = 2;
	if (!m_Buf.has(m_Cur, header_size + SIZE)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	BufferIterator_t step = m_Cur;

	--m_CurLevel->countdown;
	if constexpr ((READER::VALID_TYPES & type) == MP_NONE) {
		r.WrongType(READER::VALID_TYPES, type);
		AbortAndSkipRead(READ_WRONG_TYPE);
	} else {
		++step;
		int8_t ext_type = step.template get<int8_t>();
		r.Value(m_Cur, ctype,
			ExtValue{ext_type, header_size, SIZE});
	}
	m_Cur += header_size + SIZE;
}

namespace details {

struct TagInfo {
	uint8_t header_size : 6;
	uint8_t read_value_size : 2;
	uint8_t read_value_str_like : 1;
	uint8_t add_count : 5;
	uint8_t read_value_arr_map : 2;

	constexpr TagInfo(uint8_t a = 1, uint8_t b = 0, uint8_t c = 0,
			  uint8_t d = 0, uint8_t e = 0)
		: header_size(a), read_value_size(b), read_value_str_like(c),
		  add_count(d), read_value_arr_map(e)
	{}
};

static constexpr TagInfo tag_info[] = {
	// fixed uint x 128
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	// fix map x 16
	{1, 0, 0,  0}, {1, 0, 0,  2}, {1, 0, 0,  4}, {1, 0, 0,  6},
	{1, 0, 0,  8}, {1, 0, 0, 10}, {1, 0, 0, 12}, {1, 0, 0, 14},
	{1, 0, 0, 16}, {1, 0, 0, 18}, {1, 0, 0, 20}, {1, 0, 0, 22},
	{1, 0, 0, 24}, {1, 0, 0, 26}, {1, 0, 0, 28}, {1, 0, 0, 30},
	// fix arr x 16
	{1, 0, 0,  0}, {1, 0, 0,  1}, {1, 0, 0,  2}, {1, 0, 0,  3},
	{1, 0, 0,  4}, {1, 0, 0,  5}, {1, 0, 0,  6}, {1, 0, 0,  7},
	{1, 0, 0,  8}, {1, 0, 0,  9}, {1, 0, 0, 10}, {1, 0, 0, 11},
	{1, 0, 0, 12}, {1, 0, 0, 13}, {1, 0, 0, 14}, {1, 0, 0, 15},
	// fix str x 32
	{ 1}, { 2}, { 3}, { 4}, { 5}, { 6}, { 7}, { 8}, { 9}, {10},
	{11}, {12}, {13}, {14}, {15}, {16}, {17}, {18}, {19}, {20},
	{21}, {22}, {23}, {24}, {25}, {26}, {27}, {28}, {29}, {30},
	{31}, {32},
	// nil bas bool x 2
	{1},{1},{1},{1},
	// bin8 bin16 bin32
	{1+1, 1, 1}, {1+2, 2, 1}, {1+4, 3, 1},
	// ext8 ext16 ext32
	{2+1, 1, 1}, {2+2, 2, 1}, {2+4, 3, 1},
	// float double
	{1+4}, {1+8},
	// uint 8 - 64
	{1+1}, {1+2}, {1+4}, {1+8},
	// int 8 - 64
	{1+1}, {1+2}, {1+4}, {1+8},
	// fixext 1-16
	{2+1}, {2+2}, {2+4}, {2+8}, {2+16},
	// str8 str16 str32
	{1+1, 1, 1}, {1+2, 2, 1}, {1+4, 3, 1},
	// arr16 arr32
	{1+2, 2, 0, 0, 1}, {1+4, 3, 0, 0, 1},
	// map16 map32
	{1+2, 2, 0, 0, 2}, {1+4, 3, 0, 0, 2},
	// fix int x 32
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
	{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},
};
static_assert(std::size(tag_info) == 256, "Smth was missed?");

} // namespace details {

template <class BUFFER>
void
Dec<BUFFER>::SkipCommon()
{
	uint8_t tag = m_Cur.template get<uint8_t>();
	if (tag == 0xc1) {
		AbandonDecoder(READ_BAD_MSGPACK);
		return;
	}
	const details::TagInfo &info = details::tag_info[tag];
	if (!m_Buf.has(m_Cur, info.header_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	size_t value;
	switch (info.read_value_size) {
		case 0: value = 0; break;
		case 1:
			value = bswap((m_Cur + 1).template get<uint8_t>());
			break;
		case 2:
			value = bswap((m_Cur + 1).template get<uint16_t>());
			break;
		case 3:
			value = bswap((m_Cur + 1).template get<uint32_t>());
			break;
		default:
			tnt::unreachable();
	}
	size_t obj_size = info.header_size + value * info.read_value_str_like;
	if (!m_Buf.has(m_Cur, obj_size)) {
		m_Result = m_Result | READ_NEED_MORE;
		return;
	}
	size_t add_count = info.add_count + value * info.read_value_arr_map;
	m_CurLevel->countdown += add_count;
	--m_CurLevel->countdown;
	m_Cur += obj_size;
}

template <class DEC, class READER, class SEQUENCE>
struct ReaderMap;

template <class DEC, class READER, size_t ... N>
struct ReaderMap<DEC, READER, std::index_sequence<N...>> {
	using Transition_t = typename DEC::Transition_t;
	template <size_t I>
	static constexpr Transition_t get()
	{
		if constexpr (I <= 0x7f)
			return &DEC::template ReadUint<READER, void>;
		else if constexpr (I <= 0x8f )
			return &DEC::template ReadMap<READER, void>;
		else if constexpr (I <= 0x9f )
			return &DEC::template ReadArr<READER, void>;
		else if constexpr (I <= 0xa0 )
			return &DEC::template ReadZeroStr<READER>;
		else if constexpr (I <= 0xbf )
			return &DEC::template ReadStr<READER, void>;
		else if constexpr (I <= 0xc0)
			return &DEC::template ReadNil<READER>;
		else if constexpr (I <= 0xc1)
			return &DEC::template ReadBad<READER>;
		else if constexpr (I <= 0xc3)
			return &DEC::template ReadBool<READER>;
		else if constexpr (I <= 0xc4)
			return &DEC::template ReadBin<READER, uint8_t>;
		else if constexpr (I <= 0xc5)
			return &DEC::template ReadBin<READER, uint16_t>;
		else if constexpr (I <= 0xc6)
			return &DEC::template ReadBin<READER, uint32_t>;
		else if constexpr (I <= 0xc7)
			return &DEC::template ReadExt<READER, uint8_t>;
		else if constexpr (I <= 0xc8)
			return &DEC::template ReadExt<READER, uint16_t>;
		else if constexpr (I <= 0xc9)
			return &DEC::template ReadExt<READER, uint32_t>;
		else if constexpr (I <= 0xca)
			return &DEC::template ReadFlt<READER, float>;
		else if constexpr (I <= 0xcb)
			return &DEC::template ReadFlt<READER, double>;
		else if constexpr (I <= 0xcc)
			return &DEC::template ReadUint<READER, uint8_t>;
		else if constexpr (I <= 0xcd)
			return &DEC::template ReadUint<READER, uint16_t>;
		else if constexpr (I <= 0xce)
			return &DEC::template ReadUint<READER, uint32_t>;
		else if constexpr (I <= 0xcf)
			return &DEC::template ReadUint<READER, uint64_t>;
		else if constexpr (I <= 0xd0)
			return &DEC::template ReadInt<READER, int8_t>;
		else if constexpr (I <= 0xd1)
			return &DEC::template ReadInt<READER, int16_t>;
		else if constexpr (I <= 0xd2)
			return &DEC::template ReadInt<READER, int32_t>;
		else if constexpr (I <= 0xd3)
			return &DEC::template ReadInt<READER, int64_t>;
		else if constexpr (I <= 0xd8)
			return &DEC::template ReadFixedExt<READER, 1u << (I - 0xd4)>;
		else if constexpr (I <= 0xd9)
			return &DEC::template ReadStr<READER, uint8_t>;
		else if constexpr (I <= 0xda)
			return &DEC::template ReadStr<READER, uint16_t>;
		else if constexpr (I <= 0xdb)
			return &DEC::template ReadStr<READER, uint32_t>;
		else if constexpr (I <= 0xdc)
			return &DEC::template ReadArr<READER, uint16_t>;
		else if constexpr (I <= 0xdd)
			return &DEC::template ReadArr<READER, uint32_t>;
		else if constexpr (I <= 0xde)
			return &DEC::template ReadMap<READER, uint16_t>;
		else if constexpr (I <= 0xdf)
			return &DEC::template ReadMap<READER, uint32_t>;
		else
			return &DEC::template ReadInt<READER, void>;
		static_assert(I <= 0xff, "Wtf?");
	}
	static constexpr Transition_t transitions[256] = {get<N>()...};
};

template <class BUFFER>
template<class READER, class... ARGS>
void
Dec<BUFFER>::FillState(State &st, ARGS&&... args)
{
	// We never use the second state on top level.
	assert(&st != &m_Levels[0].state[1]);
	using Reader_t = std::decay_t<READER>;
	st.objHolder.template create<Reader_t>(std::forward<ARGS>(args)...);
	st.transitions = ReaderMap<Dec_t, Reader_t,
		std::make_index_sequence<256>>::transitions;
	st.storeEndIterator =
		st.objHolder.template get<Reader_t>().StoreEndIterator();
}

template <class BUFFER>
void
Dec<BUFFER>::FillSkipState(State &st, BufferIterator_t *save_end)
{
#define REP16(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define REP16_DELAYED(x) REP16(x)
#define REP256(x) REP16(REP16_DELAYED(x))
	static constexpr Transition_t transit[] = {REP256(&Dec_t::SkipCommon)};
	static_assert(std::size(transit) == 256, "Miss smth?");
#undef REP256
#undef REP16_DELAYED
#undef REP16
	st.transitions = transit;
	st.storeEndIterator = save_end;
}

template <class BUFFER>
void
Dec<BUFFER>::AbortAndSkipRead(ReadResult_t error)
{
	m_Result = m_Result | error;
	while (m_CurLevel != m_Levels) {
		size_t tmp = m_CurLevel->countdown;
		m_CurLevel->state[0].objHolder.destroy();
		m_CurLevel->state[1].objHolder.destroy();
		--m_CurLevel;
		m_CurLevel->countdown += tmp;
	}
	m_CurLevel->stateMask = 0;
}

template <class BUFFER>
void
Dec<BUFFER>::AbandonDecoder(ReadResult_t error)
{
	m_IsDeadStream = true;
	m_Result = m_Result | error;
}

template <class BUFFER>
template <class T, class... U>
void Dec<BUFFER>::SetReader(bool second, U&&... u)
{
	FillState<T>(m_CurLevel->state[second], std::forward<U>(u)...);
}

template <class BUFFER>
template <class T>
void Dec<BUFFER>::SetReader(bool second, T&& t)
{
	FillState<T>(m_CurLevel->state[second], std::forward<T>(t));
}

template <class BUFFER>
void Dec<BUFFER>::Skip(BufferIterator_t *saveEnd)
{
	FillSkipState(m_CurLevel->state[0], saveEnd);
	FillSkipState(m_CurLevel->state[1], saveEnd);
}

template <class BUFFER>
void Dec<BUFFER>::SetPosition(BufferIterator_t &itr)
{
	m_Cur = itr;
}


template <class BUFFER>
ReadResult_t
Dec<BUFFER>::Read()
{
	if (m_IsDeadStream)
		return m_Result;
	m_Result = m_Result & ~READ_NEED_MORE;
	if (m_CurLevel->countdown == 0) {
		m_CurLevel->countdown = 1;
		m_Result = READ_SUCCESS;
	}
	while (true) {
		if (!m_Buf.has(m_Cur, 1)) {
			m_Result = m_Result | READ_NEED_MORE;
			return m_Result;
		}
		uint8_t tag = m_Cur.template get<uint8_t>();
		(this->*(CurState().transitions[tag]))();
		if (m_IsDeadStream || (m_Result & READ_NEED_MORE))
			return m_Result;
		while (m_CurLevel->countdown == 0) {
			m_CurLevel->state[0].objHolder.destroy();
			m_CurLevel->state[1].objHolder.destroy();
			if (m_CurLevel == m_Levels)
				return m_Result;
			--m_CurLevel;
			if (LastState().storeEndIterator != nullptr)
				*LastState().storeEndIterator = m_Cur;
		}
	}
}

} // namespace mpp {
