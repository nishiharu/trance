// -*- mode: c++ -*-
//
//  Copyright(C) 2014 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __TRANCE__FEATURE_VECTOR_COMPACT__HPP__
#define __TRANCE__FEATURE_VECTOR_COMPACT__HPP__ 1

#include <memory>
#include <utility>
#include <algorithm>
#include <iterator>

#include <trance/feature.hpp>

#include <utils/hashmurmur3.hpp>
#include <utils/bithack.hpp>
#include <utils/byte_aligned_code.hpp>
#include <utils/simple_vector.hpp>

namespace trance
{
  template <typename __Tp, typename __Alloc >
  class FeatureVector;

  template <typename __Tp, typename __Alloc >
  class FeatureVectorLinear;

  // a compact feature vector representation which do not allow any modification, and
  // uses input-iterator, not bidirectional/random-access iterator
  // we use double as our underlying stroage..
  
  struct __feature_vector_feature_codec
  {
    typedef trance::Feature feature_type;
    typedef uint8_t byte_type;

    static size_t encode(byte_type* buffer, const feature_type::id_type& value)
    {
      return utils::byte_aligned_encode(value, reinterpret_cast<char*>(buffer));
    }
    
    static size_t encode(byte_type* buffer, const feature_type& value)
    {
      return encode(buffer, value.id());
    }
    
    static size_t decode(const byte_type* buffer, feature_type::id_type& value)
    {
      return utils::byte_aligned_decode(value, reinterpret_cast<const char*>(buffer));
    }
    static size_t decode(const byte_type* buffer, feature_type& value)
    {
      feature_type::id_type value_id = 0;
      const size_t ret = utils::byte_aligned_decode(value_id, reinterpret_cast<const char*>(buffer));
      value = feature_type(value_id);
      return ret;
    }
  };
  
  struct __feature_vector_data_codec
  {    
    typedef uint8_t byte_type;

    template <typename __Tp>
    static byte_type* cast(__Tp& x)
    {
      return (byte_type*) &x;
    }
    
    template <typename __Tp>
    static const byte_type* cast(const __Tp& x)
    {
      return (const byte_type*) &x;
    }
    
    static 
    size_t byte_size(const uint64_t& x)
    {
      return (1 
	      + bool(x & 0xffffffffffffff00ull)
	      + bool(x & 0xffffffffffff0000ull)
	      + bool(x & 0xffffffffff000000ull)
	      + bool(x & 0xffffffff00000000ull)
	      + bool(x & 0xffffff0000000000ull)
	      + bool(x & 0xffff000000000000ull)
	      + bool(x & 0xff00000000000000ull));
    }

    static size_t encode(byte_type* buffer, const double& value)
    {
      static const uint8_t mask_float    = 1 << (4 + 0);
      static const uint8_t mask_unsigned = 1 << (4 + 1);
      static const uint8_t mask_signed   = 1 << (4 + 2);
      static const uint8_t mask_size     = 0x0f;
      
      if (::fmod(value, 1.0) == 0.0) {
	const int64_t  val = value;
	const uint64_t value_encode = utils::bithack::branch(val < 0, - val, val);
	const size_t   value_size = byte_size(value_encode);
	
	*buffer = utils::bithack::branch(val < 0, mask_signed, mask_unsigned) | (value_size & mask_size);
	++ buffer;
	
	switch (value_size) {
	case 8: buffer[value_size - 8] = (value_encode >> 56);
	case 7: buffer[value_size - 7] = (value_encode >> 48);
	case 6: buffer[value_size - 6] = (value_encode >> 40);
	case 5: buffer[value_size - 5] = (value_encode >> 32);
	case 4: buffer[value_size - 4] = (value_encode >> 24);
	case 3: buffer[value_size - 3] = (value_encode >> 16);
	case 2: buffer[value_size - 2] = (value_encode >> 8);
	case 1: buffer[value_size - 1] = (value_encode);
	}
	
	return value_size + 1;
      } else {
	*buffer = (mask_float | (sizeof(double) & mask_size));
	++ buffer;
	
	std::copy(cast(value), cast(value) + sizeof(double), buffer);
	
	return sizeof(double) + 1;
      }
    }
    
    static
    size_t decode(const byte_type* buffer, double& value) 
    {
      static const uint8_t mask_float    = 1 << (4 + 0);
      static const uint8_t mask_unsigned = 1 << (4 + 1);
      static const uint8_t mask_signed   = 1 << (4 + 2);
      static const uint8_t mask_size     = 0x0f;

      if (*buffer & mask_float) {
	++ buffer;
	std::copy(buffer, buffer + sizeof(double), cast(value));
	return sizeof(double) + 1;
      } else if ((*buffer & mask_signed) || (*buffer & mask_unsigned)) {
	const bool value_signed = (*buffer & mask_signed);
	const size_t value_size = (*buffer & mask_size);

	++ buffer;
	
	const uint64_t mask = 0xff;
	uint64_t value_decode = 0;
	switch (value_size) {
	case 8: value_decode |= ((uint64_t(buffer[value_size - 8]) & mask) << 56);
	case 7: value_decode |= ((uint64_t(buffer[value_size - 7]) & mask) << 48);
	case 6: value_decode |= ((uint64_t(buffer[value_size - 6]) & mask) << 40);
	case 5: value_decode |= ((uint64_t(buffer[value_size - 5]) & mask) << 32);
	case 4: value_decode |= ((uint64_t(buffer[value_size - 4]) & mask) << 24);
	case 3: value_decode |= ((uint64_t(buffer[value_size - 3]) & mask) << 16);
	case 2: value_decode |= ((uint64_t(buffer[value_size - 2]) & mask) << 8);
	case 1: value_decode |= ((uint64_t(buffer[value_size - 1]) & mask));
	}
	
	value = utils::bithack::branch(value_signed, - int64_t(value_decode), int64_t(value_decode));
	
	return value_size + 1;
      } else
	throw std::runtime_error("invalid type for decoding");
    }
  };

  
  class FeatureVectorCompact
  {
  public:
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    
    typedef trance::Feature feature_type;
    typedef trance::Feature key_type;
    typedef double mapped_type;
    typedef double data_type;
    
    typedef std::pair<const feature_type, data_type> value_type;

    typedef uint8_t byte_type;
        
  private:
    typedef utils::simple_vector<byte_type, std::allocator<byte_type> > impl_type;
    
  public:
    typedef __feature_vector_feature_codec codec_key_type;
    typedef __feature_vector_feature_codec codec_feature_type;
    typedef __feature_vector_data_codec    codec_data_type;
    typedef __feature_vector_data_codec    codec_mapped_type;
    
  public:
    struct iterator
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      typedef std::input_iterator_tag   iterator_category;
      typedef std::pair<const feature_type, data_type> value_type;
      typedef const value_type* pointer;
      typedef const value_type& reference;
      
      typedef const byte_type* ptr_type;
      
    public:
      iterator(ptr_type iter, ptr_type last) : iter_(iter), last_(last), impl_()
      {
	if (iter_ != last_) {
	  const size_type feature_size = codec_feature_type::decode(&(*iter_), const_cast<feature_type&>(impl_.first));
	  iter_ += feature_size;
	  
	  const size_type data_size = codec_data_type::decode(&(*iter_), const_cast<data_type&>(impl_.second));
	  iter_ += data_size;
	} else {
	  iter_ = 0;
	  last_ = 0;
	}
      }
      
      iterator() : iter_(0), last_(0), impl_() {}
      iterator(const iterator& x) : iter_(x.iter_), last_(x.last_), impl_(x.impl_) {}
      iterator& operator=(const iterator& x)
      {
	iter_ = x.iter_;
	last_ = x.last_;
	const_cast<feature_type&>(impl_.first) = x.impl_.first;
	const_cast<data_type&>(impl_.second) = x.impl_.second;
	
	return *this;
      }
      
      const value_type& operator*() const { return impl_; }
      const value_type* operator->() const { return &impl_; }
      
      iterator& operator++()
      {
	increment();
	return *this;
      }
      
      iterator operator++(int)
      {
	iterator tmp = *this;
	increment();
	return tmp;
      }
      
      friend
      bool operator==(const iterator& x, const iterator& y)
      {
	return (x.iter_ == y.iter_) && (x.last_ == y.last_);
      }
      
      friend
      bool operator!=(const iterator& x, const iterator& y)
      {
	return (x.iter_ != y.iter_) || (x.last_ != y.last_);
      }
      
    private:
      void increment()
      {
	if (iter_ == last_) {
	  iter_ = 0;
	  last_ = 0;
	} else {
	  feature_type::id_type feature_inc = 0;
	  const size_type feature_size = codec_feature_type::decode(&(*iter_), feature_inc);
	  const_cast<feature_type&>(impl_.first) = feature_type(impl_.first.id() + feature_inc);
	  iter_ += feature_size;
	  
	  const size_type data_size = codec_data_type::decode(&(*iter_), const_cast<data_type&>(impl_.second));
	  iter_ += data_size;
	}
      }

    private:
      ptr_type   iter_;
      ptr_type   last_;
      value_type impl_;
    };
    
    typedef iterator const_iterator;
    
    typedef const value_type& reference;
    typedef const value_type& const_reference;
    
    struct encoder_type
    {
      template <typename Iterator, typename Output>
      Output operator()(Iterator first, Iterator last, Output iter) const
      {
	feature_type::id_type id_prev = 0;
	for (/**/; first != last; ++ first) {
	  const feature_type::id_type id = feature_type(first->first).id();
	  
	  std::advance(iter, codec_feature_type::encode(&(*iter), id - id_prev));
	  std::advance(iter, codec_data_type::encode(&(*iter), first->second));
	  
	  id_prev = id;
	}
	
	return iter;
      }
    };

  private:
    template <typename Tp>
    struct less_first
    {
      bool operator()(const Tp& x, const Tp& y) const
      {
	return x.first < y.first;
      }
    };
    
  public:
    FeatureVectorCompact() {}
    
    template <typename Iterator>
    FeatureVectorCompact(Iterator first, Iterator last, const bool sorted=false) { assign(first, last, sorted); }
    
    template <typename T, typename A>
    FeatureVectorCompact(const FeatureVector<T, A>& x) { assign(x); }
    
    template <typename T, typename A>
    FeatureVectorCompact(const FeatureVectorLinear<T, A>& x) { assign(x); }

    FeatureVectorCompact(const FeatureVectorCompact& x) : impl_(x.impl_) {}

    FeatureVectorCompact& operator=(const FeatureVectorCompact& x)
    {
      impl_ = x.impl_;
      return *this;
    }
    
    template <typename T, typename A>
    FeatureVectorCompact& operator=(const FeatureVector<T, A>& x)
    {
      assign(x);
      return *this;
    }

    template <typename T, typename A>
    FeatureVectorCompact& operator=(const FeatureVectorLinear<T, A>& x)
    {
      assign(x);
      return *this;
    }

    void assign(const FeatureVectorCompact& x)
    {
      impl_.assign(x.impl_);
    }
    
    template <typename Iterator>
    void assign(Iterator first, Iterator last, const bool sorted=false)
    {
      typedef std::pair<feature_type, double> pair_type;
      typedef std::vector<pair_type, std::allocator<pair_type> > raw_type;
      typedef std::vector<byte_type, std::allocator<byte_type> > compressed_type;

      encoder_type encoder;

      if (sorted) {
	compressed_type compressed(std::distance(first, last) * 16);
	
	impl_.assign(compressed.begin(), encoder(first, last, compressed.begin()));
      } else {
	raw_type raw(first, last);
	std::sort(raw.begin(), raw.end(), less_first<pair_type>());
	
	compressed_type compressed(raw.size() * 16);
	
	impl_.assign(compressed.begin(), encoder(raw.begin(), raw.end(), compressed.begin()));
      }
    }
    
    template <typename T, typename A>
    void assign(const FeatureVector<T, A>& x)
    {
      typedef std::pair<feature_type, double> pair_type;
      typedef std::vector<pair_type, std::allocator<pair_type> > raw_type;
      typedef std::vector<byte_type, std::allocator<byte_type> > compressed_type;
      
      raw_type raw(x.begin(), x.end());
      std::sort(raw.begin(), raw.end(), less_first<pair_type>());
      
      encoder_type encoder;
      compressed_type compressed(raw.size() * 16);
      
      impl_.assign(compressed.begin(), encoder(raw.begin(), raw.end(), compressed.begin()));
    }

    template <typename T, typename A>
    void assign(const FeatureVectorLinear<T, A>& x)
    {
      typedef std::vector<byte_type, std::allocator<byte_type> > compressed_type;

      encoder_type encoder;
      compressed_type compressed(x.size() * 16);
      
      impl_.assign(compressed.begin(), encoder(x.begin(), x.end(), compressed.begin()));
    }

  public:
    const_iterator begin() const { return const_iterator(&(*impl_.begin()), &(*impl_.end())); }
    const_iterator end() const { return const_iterator(); }
    
    bool empty() const { return impl_.empty(); }
    size_type size_compressed() const  { return impl_.size(); }

    void clear() { impl_.clear(); }

    void swap(FeatureVectorCompact& x)
    {
      impl_.swap(x.impl_);
    }
    
  public:
    friend size_t hash_value(FeatureVectorCompact const& x) { return utils::hashmurmur3<size_t>()(x.impl_.begin(), x.impl_.end(), 0); }
    
    friend bool operator==(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ == y.impl_; }
    friend bool operator!=(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ != y.impl_; }
    friend bool operator<(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ < y.impl_; }
    friend bool operator>(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ > y.impl_; }
    friend bool operator<=(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ <= y.impl_; }
    friend bool operator>=(const FeatureVectorCompact& x, const FeatureVectorCompact& y) { return x.impl_ >= y.impl_; }
    
  private:
    impl_type impl_;
  }; 
};

namespace std
{
  inline
  void swap(trance::FeatureVectorCompact& x, trance::FeatureVectorCompact& y)
  {
    x.swap(y);
  }
};


#include <trance/feature_vector.hpp>
#include <trance/feature_vector_linear.hpp>

#endif
