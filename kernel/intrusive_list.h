#pragma once

namespace util
{
    template<typename T> struct intrusive_list;

    namespace detail
    {
        class intrusive_list_base;

        class intrusive_list_node {
            friend class intrusive_list_base;
            template<typename T> friend struct ::util::intrusive_list;

            intrusive_list_node* prev = nullptr;
            intrusive_list_node* next = nullptr;
        };

        struct Forward { };
        struct Backward { };

        struct intrusive_list_base {
            intrusive_list_node* head{};
            intrusive_list_node* tail{};

            void pop_front()
            {
                head = head->next;
                if (head)
                    head->prev = nullptr;
            }

            void pop_back()
            {
                tail = tail->prev;
                if (tail)
                    tail->next = nullptr;
            }

            bool empty() const { return head == nullptr; }
            void clear()
            {
                head = nullptr;
                tail = nullptr;
            }

            intrusive_list_node* Successor(Forward, intrusive_list_node* p) const
            {
                return p ? p->next : head;
            }

            intrusive_list_node* Predecessor(Forward, intrusive_list_node* p) const
            {
                return p ? p->prev : tail;
            }

            intrusive_list_node* Successor(Backward, intrusive_list_node* p) const
            {
                return Predecessor(Forward{}, p);
            }

            intrusive_list_node* Predecessor(Backward, intrusive_list_node* p) const
            {
                return Successor(Forward{}, p);
            }
        };

        template<typename T, typename Direction>
        class list_iterator {
            const intrusive_list_base& list;
            intrusive_list_node* node;
        public:
            list_iterator(const intrusive_list_base& list, intrusive_list_node* p) : list(list), node(p) {}

            list_iterator& operator++()
            {
                node = list.Successor(Direction{}, node);
                return *this;
            }

            list_iterator operator++(int)
            {
                auto s(*this);
                node = list.Successor(Direction{}, node);
                return s;
            }

            list_iterator& operator--()
            {
                node = list.Predecessor(Direction{}, node);
                return *this;
            }

            list_iterator operator--(int)
            {
                auto s(*this);
                node = list.Predecessor(Direction{}, node);
                return s;
            }

            T* operator->() const { return static_cast<T*>(node); }

            T& operator*() const { return *operator->(); }

            bool operator==(const list_iterator& rhs) const
            {
                return node == rhs.node && &list == &rhs.list;
            }

            bool operator!=(const list_iterator& rhs) const { return !(*this == rhs); }
        };
    } // namespace detail

    using intrusive_list_node = detail::intrusive_list_node;

    /*
     * This implements a standard doubly-linked list structure; obtaining/removing
     * the head takes O(1), adding a new item takes O(1) and removing any item
     * takes O(1).  Iterating through the entire list takes O(n) (this also holds
     * for locating a single item).
     *
     * Each list has a 'head' and 'tail' element, and every item has a previous
     * and next pointer (contained in intrusive_list_node).
     */
    template<typename T>
    struct intrusive_list : detail::intrusive_list_base {
        using iterator = typename detail::list_iterator<T, detail::Forward>;
        using const_iterator = typename detail::list_iterator<const T, detail::Forward>;
        using reverse_iterator = typename detail::list_iterator<T, detail::Backward>;
        using const_reverse_iterator = typename detail::list_iterator<const T, detail::Backward>;

        void push_back(T& item)
        {
            item.next = nullptr;
            if (head) {
                item.prev = tail;
                tail->next = &item;
            } else {
                item.prev = nullptr;
                head = &item;
            }
            tail = &item;
        }

        void push_front(T& item)
        {
            item.prev = nullptr;
            if (head) {
                item.next = head;
                head->prev = &item;
            } else {
                item.next = nullptr;
                tail = &item;
            }
            head = &item;
        }

        void insert_before(T& pos, T& item)
        {
            if (auto pos_prev = pos.prev; pos_prev)
                pos_prev->next = &item;
            else
                head = &item;
            item.next = &pos;
            item.prev = pos.prev;
            pos.prev = &item;
        }

        void remove(T& item)
        {
            if (auto prev = item.prev; prev)
                prev->next = item.next;
            if (auto next = item.next; next)
                next->prev = item.prev;
            if (head == &item)
                head = item.next;
            if (tail == &item)
                tail = item.prev;
        }

        T& front() { return *static_cast<T*>(head); }

        const T& front() const { return *static_cast<T*>(head); }

        T& back() { return *static_cast<T*>(tail); }

        const T& back() const { return *static_cast<T*>(tail); }

        constexpr intrusive_list() = default;
        intrusive_list(const intrusive_list&) = delete;
        intrusive_list& operator=(const intrusive_list&) = delete;

        auto begin() { return iterator(*this, static_cast<T*>(head)); }
        auto begin() const { return const_iterator(*this, static_cast<T*>(head)); }

        auto rbegin() { return reverse_iterator(*this, static_cast<T*>(tail)); }
        auto rbegin() const { return const_reverse_iterator(*this, static_cast<T*>(tail)); }

        auto end() { return iterator(*this, nullptr); }
        auto end() const { return const_iterator(*this, nullptr); }

        auto rend() { return reverse_iterator(*this, nullptr); }
        auto rend() const { return const_reverse_iterator(*this, nullptr); }
    };

} // namespace util
