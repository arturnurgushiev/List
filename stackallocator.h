template <size_t N>
class StackStorage {
 private:
  char storage_[N];
  void* top_ = storage_;
  size_t free_ = N;

 public:
  StackStorage(const StackStorage&) = delete;
  StackStorage& operator=(const StackStorage&) = delete;
  StackStorage() = default;
  ~StackStorage() = default;
  void* allocate(size_t count, std::size_t a, size_t size_of_T) {
    if (std::align(a, size_of_T * count, top_, free_)) {
      void* result = top_;
      top_ = (char*)top_ + size_of_T * count;
      free_ -= size_of_T * count;
      return result;
    }
    return nullptr;
  }
};

template <typename T, size_t N>
class StackAllocator {
  template <typename U, size_t M>
  friend class StackAllocator;

 private:
  StackStorage<N>* stack_storage_;

 public:
  using value_type = T;
  T* allocate(size_t count, std::size_t a = alignof(T)) {
    return reinterpret_cast<T*>(stack_storage_->allocate(count, a, sizeof(T)));
  }

  void deallocate(T*, size_t) {}

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  template <typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& other) {
    stack_storage_ = other.stack_storage_;
  }

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& other)
      : stack_storage_(other.stack_storage_) {}
  StackAllocator(StackStorage<N>& storage) : stack_storage_(&storage) {}
  StackAllocator() = default;
  ~StackAllocator() = default;
};

template <typename T, typename Alloc = std::allocator<T>>
class List {
 private:
  struct BaseNode {
    BaseNode* prev;
    BaseNode* next;
    BaseNode() = default;
    BaseNode(const BaseNode&) = default;
  };

  struct Node : BaseNode {
    T value;
  };

  size_t sz_;

  BaseNode fakeNode_;

  using NodeAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
  using NodeTraits = std::allocator_traits<NodeAlloc>;
  [[no_unique_address]] NodeAlloc nodeAlloc_;
  template <bool IsConst>
  class BasicIterator {
   public:
    using difference_type = int;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    using reference = std::conditional_t<IsConst, const T&, T&>;
    using value_type = T;
    using iterator_category = std::bidirectional_iterator_tag;

   private:
    List::BaseNode* ptr_;
    friend class List;

   public:
    operator BasicIterator<true>() const { return {ptr_}; }

    BasicIterator() = default;

    BasicIterator(List::BaseNode* ptr) : ptr_(ptr) {}

    reference operator*() const { return static_cast<Node*>(ptr_)->value; }

    pointer operator->() const { return &(static_cast<Node*>(ptr_)->value); }

    BasicIterator& operator++() {
      ptr_ = ptr_->next;
      return *this;
    }

    BasicIterator& operator--() {
      ptr_ = ptr_->prev;
      return *this;
    }

    BasicIterator operator++(int) {
      BasicIterator copy = *this;
      ++*this;
      return copy;
    }

    BasicIterator operator--(int) {
      BasicIterator copy = *this;
      --*this;
      return copy;
    }

    bool operator==(BasicIterator other) { return ptr_ == other.ptr_; }
  };

 public:
  using iterator = BasicIterator<false>;
  using const_iterator = BasicIterator<true>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  iterator begin() { return iterator(fakeNode_.next); }

  const_iterator begin() const { return cbegin(); }

  iterator end() { return iterator(&fakeNode_); }

  const_iterator end() const { return cend(); }

  const_iterator cbegin() const { return const_iterator(fakeNode_.next); }

  const_iterator cend() const { return const_iterator(fakeNode_.next->prev); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }

  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator rbegin() const { return crbegin(); }

  const_reverse_iterator rend() const { return crend(); }

  size_t size() const { return sz_; }

  void push_back(const T& value) {
    insert(end(), value);
  }

  void push_front(const T& value) {
    insert(begin(), value);
  }

  void pop_back() { erase(--end()); }

  void pop_front() { erase(begin()); }

  void insert(const_iterator it, const T& value) {
    Node* new_node = NodeTraits::allocate(nodeAlloc_, 1);
    try {
      NodeTraits::construct(nodeAlloc_, &new_node->value, value);
      it.ptr_->prev->next = new_node;
      new_node->prev = it.ptr_->prev;
      it.ptr_->prev = new_node;
      new_node->next = it.ptr_;
      ++sz_;
    } catch(...) {
      NodeTraits::deallocate(nodeAlloc_, new_node, 1);
      throw;
    }
  }

  void erase(const_iterator it) {
    it.ptr_->prev->next = it.ptr_->next;
    it.ptr_->next->prev = it.ptr_->prev;
    NodeTraits::destroy(nodeAlloc_, &(static_cast<Node*>(it.ptr_)->value));
    NodeTraits::deallocate(nodeAlloc_, static_cast<Node*>(it.ptr_), 1);
    --sz_;
  }

  List() : sz_(0) {
    fakeNode_.prev = &fakeNode_;
    fakeNode_.next = &fakeNode_;
  }

  List(const Alloc& alloc) : sz_(0), nodeAlloc_(alloc) {
    fakeNode_.prev = &fakeNode_;
    fakeNode_.next = &fakeNode_;
  }

  List(const List& other, const Alloc& alloc) : List(alloc) {
    for (auto& el : other) {
      push_back(el);
    }
  }

  List(const List& other) : List() {
    nodeAlloc_ = std::allocator_traits<NodeAlloc>::select_on_container_copy_construction(
        other.nodeAlloc_);
    for (auto& el : other) {
      push_back(el);
    }
  }

  List(size_t sz) : List() {
    Node* last_node;
    try {
      for (size_t i = 0; i < sz; ++i) {
        Node* new_node = NodeTraits::allocate(nodeAlloc_, 1);
        last_node = new_node;
        NodeTraits::construct(nodeAlloc_, &new_node->value);
        new_node->next = &fakeNode_;
        new_node->prev = fakeNode_.prev;
        fakeNode_.prev->next = new_node;
        fakeNode_.prev = new_node;
        ++sz_;
      }
    } catch (...) {
      NodeTraits::deallocate(nodeAlloc_, last_node, 1);
      while (sz_ > 0) {
        erase(begin());
      }
      throw;
    }
  }

  List(size_t sz, Alloc& alloc) : List(alloc) {
    sz_ = sz;
    for (size_t i = 0; i < sz; ++i) {
      Node* new_node = NodeTraits::allocate(nodeAlloc_, 1);
      NodeTraits::construct(nodeAlloc_, &new_node->value);
      new_node->next = &fakeNode_;
      new_node->prev = fakeNode_.prev;
      fakeNode_.prev->next = new_node;
      fakeNode_.prev = new_node;
    }
  }

  List& operator=(const List& other) {
    NodeAlloc new_alloc = std::allocator_traits<
                              NodeAlloc>::propagate_on_container_copy_assignment::value
                              ? other.nodeAlloc_
                              : nodeAlloc_;
    List new_list(other, new_alloc);
    std::swap(fakeNode_.next, new_list.fakeNode_.next);
    std::swap(fakeNode_.prev, new_list.fakeNode_.prev);
    fakeNode_.prev->next = &fakeNode_;
    fakeNode_.next->prev = &fakeNode_;
    new_list.fakeNode_.prev->next = &new_list.fakeNode_;
    new_list.fakeNode_.next->prev = &new_list.fakeNode_;
    std::swap(sz_, new_list.sz_);
    std::swap(nodeAlloc_, new_list.nodeAlloc_);
    return *this;
  }

  List(size_t sz, const T& value) : List() {
    Node* last_node;
    try {
      for (size_t i = 0; i < sz; ++i) {
        Node* new_node = NodeTraits::allocate(nodeAlloc_, 1);
        last_node = new_node;
        NodeTraits::construct(nodeAlloc_, &new_node->value, value);
        new_node->next = &fakeNode_;
        new_node->prev = fakeNode_.prev;
        fakeNode_.prev->next = new_node;
        fakeNode_.prev = new_node;
        ++sz_;
      }
    } catch (...) {
      NodeTraits::deallocate(nodeAlloc_, last_node, 1);
      while (sz_ > 0) {
        erase(begin());
      }
      throw;
    }
  }

  List(size_t sz, const T& value, Alloc alloc) : List(alloc) {
    Node* last_node;
    try {
      for (size_t i = 0; i < sz; ++i) {
        Node* new_node = NodeTraits::allocate(nodeAlloc_, 1);
        last_node = new_node;
        NodeTraits::construct(nodeAlloc_, &new_node->value, value);
        new_node->next = &fakeNode_;
        new_node->prev = fakeNode_.prev;
        fakeNode_.prev->next = new_node;
        fakeNode_.prev = new_node;
        ++sz_;
      }
    } catch (...) {
      NodeTraits::deallocate(nodeAlloc_, last_node, 1);
      while (sz_ > 0) {
        erase(begin());
      }
      throw;
    }
  }

  ~List() {
    while (sz_ > 0) {
      erase(begin());
    }
  }

  Alloc get_allocator() { return Alloc(nodeAlloc_); }
};