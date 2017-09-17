// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#pragma once

#include "../SRPlatform/BaseBucketSummator.h"
#include "../SRPlatform/Interface/SRPoolRunner.h"
#include "../SRPlatform/Interface/SRNumHelper.h"
#include "../SRPlatform/BucketerTask.h"
#include "../SRPlatform/BucketerSubtaskSum.h"

namespace SRPlat {

template<typename taNumber> class SRBucketSummatorPar : public BaseBucketSummator<taNumber> {
  friend class BucketerSubtaskSum<taNumber>;

  taNumber *_pWorkerSums;
  const SRThreadCount _nWorkers;

private: // methods
  inline taNumber& ModBucket(const SRThreadCount iWorker, const uint32_t iBucket);
  inline static __m128i __vectorcall OffsetsFrom4Exps(const SRThreadCount iWorker, const __m128i exps32);
  inline static SRPacked64 __vectorcall Get2Offsets(const SRThreadCount iWorker, const __m128d nums);
  inline taNumber __vectorcall SumWorkerSums(const SRThreadCount nSubtasks);
  inline taNumber* GetWorkerRow(const SRThreadCount iWorker) const;
  inline const SRNumPack<taNumber>& GetVect(const SRThreadCount iWorker, const uint32_t iVect) const;

public: // methods
  static inline size_t GetMemoryRequirementBytes(const SRThreadCount nWorkers);
  explicit inline SRBucketSummatorPar(const SRThreadCount nWorkers, void* pMem);

  // Let each worker zero its buckets, so that they lay into L1/L2 cache of this worker.
  void inline ZeroBuckets(const  SRThreadCount iWorker);

  //// Passing |np| by value is only efficient so long as it fits and AVX register. There can be a separate function
  ////   without __vectorcall to take it by const reference.
  inline void __vectorcall CalcAdd(const SRThreadCount iWorker, const SRNumPack<taNumber> np);
  inline void __vectorcall Add(const SRThreadCount iWorker, const SRNumPack<taNumber> np, const __m256i exps);

  // Add a vector, in which only first nValid components are valid (e.g. the rest is out of range).
  inline void __vectorcall CalcAdd(const SRThreadCount iWorker, const SRNumPack<taNumber> np,
    const SRVectCompCount nValid);

  inline taNumber ComputeSum(SRPoolRunner &pr);
};

template<typename taNumber> inline taNumber* SRBucketSummatorPar<taNumber>::GetWorkerRow(
  const SRThreadCount iWorker) const
{
  return SRCast::Ptr<taNumber>(SRCast::Ptr<uint8_t>(_pBuckets) + iWorker * WorkerRowLengthBytes());
}

template<typename taNumber> inline size_t SRBucketSummatorPar<taNumber>::GetMemoryRequirementBytes(
  const SRThreadCount nWorkers)
{
  const size_t ans = size_t(nWorkers) * WorkerRowLengthBytes() + SRSimd::GetPaddedBytes(nWorkers * sizeof(taNumber));
  assert(ans < std::numeric_limits<int32_t>::max());
  return ans;
}

template<typename taNumber> inline SRBucketSummatorPar<taNumber>::SRBucketSummatorPar(
  const SRThreadCount nWorkers, void* pMem) : BaseBucketSummator(pMem), _nWorkers(nWorkers)
{
  assert((uintptr_t(pMem) & SRSimd::_cByteMask) == 0);
  _pWorkerSums = SRCast::Ptr<taNumber>(GetWorkerRow(nWorkers));
}

template<typename taNumber> inline taNumber& SRBucketSummatorPar<taNumber>::ModBucket(
  const SRThreadCount iWorker, const uint32_t iBucket)
{
  return GetWorkerRow(iWorker)[iBucket];
}

template<typename taNumber> inline __m128i __vectorcall SRBucketSummatorPar<taNumber>::OffsetsFrom4Exps(
  const SRThreadCount iWorker, const __m128i exps32)
{
  const __m128i scaled = _mm_mul_epi32(exps32, taNumber::_cSizeBytes128_32);
  const __m128i offsets = _mm_add_epi32(scaled, _mm_set1_epi32(iWorker * WorkerRowLengthBytes()));
  return offsets;
}

template<typename taNumber> inline SRPacked64 __vectorcall SRBucketSummatorPar<taNumber>::Get2Offsets(
  const SRThreadCount iWorker, const __m128d nums)
{
  const SRPacked64 exps = SRSimd::ExtractExponents32<false>(nums);
  const SRPacked64 scaled(exps._u64 * sizeof(taNumber));
  const SRPacked64 offsets(scaled._u64 + SRPacked64::Set1U32(iWorker * WorkerRowLengthBytes())._u64);
  return offsets;
}

template<typename taNumber> taNumber SRBucketSummatorPar<taNumber>::ComputeSum(SRPoolRunner &pr) {
  int64_t iPartial;
  SRVectCompCount nValid;
  const size_t nVects = SRNumHelper::Vectorize<taNumber>(BucketCount(), iPartial, nValid);
  BucketerTask<taNumber> task(pr.GetThreadPool(), *this, iPartial, nValid);
  const SRThreadCount nSubtasks = pr.SplitAndRunSubtasks<BucketerSubtaskSum<taNumber>>(task, nVects, _nWorkers)
    .GetNSubtasks();
  return SumWorkerSums(nSubtasks);
}

template<typename taNumber> inline const SRNumPack<taNumber>& SRBucketSummatorPar<taNumber>::GetVect(
  const SRThreadCount iWorker, const uint32_t iVect) const
{
  return SRCast::Ptr<SRNumPack<taNumber>>(GetWorkerRow(iWorker))[iVect];
}

/////////////////////////////////// SRBucketSummator<SRDoubleNumber> implementation ////////////////////////////////////

template<> inline void SRBucketSummatorPar<SRDoubleNumber>::ZeroBuckets(const  SRThreadCount iWorker) {
  SRUtils::FillZeroVects<true>(SRCast::Ptr<__m256i>(GetWorkerRow(iWorker)),
    WorkerRowLengthBytes() >> SRSimd::_cLogNBytes);
}

template<> inline void __vectorcall SRBucketSummatorPar<SRDoubleNumber>::CalcAdd(const SRThreadCount iWorker,
  const SRNumPack<SRDoubleNumber> np)
{
  const __m128i offsets = OffsetsFrom4Exps(iWorker, SRSimd::ExtractExponents32<false>(np._comps));
  AddInternal4(np, offsets);
}

template<> inline void __vectorcall SRBucketSummatorPar<SRDoubleNumber>::Add(const SRThreadCount iWorker,
  const SRNumPack<SRDoubleNumber> np, const __m256i exps64)
{
  const __m128i offsets = OffsetsFrom4Exps(iWorker, SRSimd::ExtractEven(exps64));
  AddInternal4(np, offsets);
}

template<> inline void __vectorcall SRBucketSummatorPar<SRDoubleNumber>::CalcAdd(const SRThreadCount iWorker,
  const SRNumPack<SRDoubleNumber> np, const typename SRVectCompCount nValid)
{
  assert(nValid <= 4);
  switch (nValid) {
  case 0:
    return;
  case 4:
    return CalcAdd(iWorker, np); // full vector addition
  case 1: {
    const int16_t exponent = SRNumTraits<double>::ExtractExponent<false>(np._comps.m256d_f64[0]);
    ModBucket(iWorker, exponent) += np._comps.m256d_f64[0];
    return;
  }

  case 3: {
    const int16_t exponent = SRNumTraits<double>::ExtractExponent<false>(np._comps.m256d_f64[2]);
    ModBucket(iWorker, exponent) += np._comps.m256d_f64[2];
  }
  // Fall through
  case 2: {
    SRPacked64 offsets = Get2Offsets(iWorker, _mm256_castpd256_pd128(np._comps));
    SRDoubleNumber &b0 = ModOffs(offsets._u32[0]);
    SRDoubleNumber &b1 = ModOffs(offsets._u32[1]);
    const __m128d old = _mm_set_pd(b1.GetValue(), b0.GetValue());
    const __m128d sums = _mm_add_pd(old, _mm256_castpd256_pd128(np._comps));
    b1.SetValue(sums.m128d_f64[1]);
    b0.SetValue(sums.m128d_f64[0]);
    break;
  }
  default:
    __assume(0);
  }
}

template<> inline SRDoubleNumber __vectorcall SRBucketSummatorPar<SRDoubleNumber>::SumWorkerSums(
  const SRThreadCount nSubtasks)
{
  return SRDoubleNumber(SRSimd::StableSum<false>(SRCast::CPtr<double>(_pWorkerSums), nSubtasks));
}

} // namespace SRPlat