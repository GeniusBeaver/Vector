#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept { 
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    
    RawMemory& operator=(RawMemory&& rhs) noexcept  { 
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
}; 

template <typename T>
class Vector {
public:
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    Vector() = default;
    
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept 
       : data_(std::exchange(other.data_, RawMemory<T>{})),
        size_(std::exchange(other.size_, 0))
    {

    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        
        RawMemory<T> new_data(new_capacity);
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector copy = rhs;
                Swap(copy);
            }
            else {              
                if (rhs.size_ < size_) {
                    size_t ts = size_ - rhs.size_;
                    for (size_t id = 0; id < rhs.size_; ++id) {
                        data_[id] = rhs.data_[id];
                    }                       
                        std::destroy_n(data_.GetAddress() + rhs.size_, ts);
                }
                else {
                    size_t ts = rhs.size_ - size_;
                    for (size_t id = 0; id < size_; ++id) {
                        data_[id] = rhs.data_[id];
                    }                       
                    std::uninitialized_copy_n(data_.GetAddress() + size_, ts, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::exchange(rhs.data_, RawMemory<T>{});
            size_ = std::exchange(rhs.size_, 0);
        }
        return *this;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        auto temp_size = size_;
        size_ = other.size_;
        other.size_ = temp_size;
    }
    
    void Resize(size_t new_size) {
        if (data_.Capacity() > new_size) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else if (data_.Capacity() < new_size) {
            Reserve(new_size); 
            std::uninitialized_value_construct_n(data_.GetAddress(), new_size - size_); 
        }
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    
     void PushBack(T&& value) {
        EmplaceBack(std::forward<T>(value));    
    }
    
    void PopBack() {
        data_[size_ - 1].~T();
        --size_;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *(Emplace(begin()+size_, std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t begin1 = std::distance(cbegin(), pos);
        
        if (size_ == data_.Capacity()) {
            size_t new_size = (size_ == 0 ? 1 : size_ * 2);
            size_t end1 = std::distance(pos, cend());
            RawMemory<T> tv(new_size);
            new (tv.GetAddress() + begin1) T(std::forward<Args>(args)...);
            
      
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), begin1, tv.GetAddress());
                std::uninitialized_move_n(begin() + begin1, end1, tv.GetAddress() + begin1 + 1);
            } else {
                std::uninitialized_copy_n(begin(), begin1, tv.GetAddress());
                std::uninitialized_copy_n(begin() + begin1, end1, tv.GetAddress() + begin1 + 1);
            }
            std::destroy_n(begin(), size_);
            data_.Swap(tv);
            
        } else {
            if (size_ != begin1) {
                new (end()) T(std::forward<T>(*(end() - 1)));
                std::move_backward(begin() + begin1, end() - 1, end());
                data_[begin1] = T(std::forward<Args>(args)...);
            } else {
                new (begin() + begin1) T(std::forward<Args>(args)...);
            }
        }
        
        ++size_;
        return begin() + begin1;
    }
    
    iterator Erase(const_iterator pos) {
        size_t pos1 = std::distance(cbegin(), pos);
        std::move(begin() + pos1 + 1, end(), begin() + pos1);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + pos1;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
    ~Vector() {
        std::destroy(data_.GetAddress(), data_.GetAddress() + size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
