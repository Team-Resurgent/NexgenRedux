#pragma once

#include "base/grow_array.h"

#include <stddef.h>

namespace nx
{
    // A whole JSON document, for files with things inside things. Comments (//) are allowed, as people
    // write these by hand.
    class JsonTree : NonCopyable
    {
    public:
        enum class Type : u8 { Null, Bool, Number, String, Array, Object };

        struct Node
        {
            Type type = Type::Null;
            bool flag = false;
            double number = 0;
            u32 text = 0;    // into the text
            u32 key = 0;     // of its place in an object
            u32 first = 0;   // child; 0 is none, as the root is nobody's child
            u32 next = 0;
            u32 count = 0;   // of children
            u32 line = 0;
        };

        // A place in the tree. One that isn't there behaves as null, so lookups can be chained.
        class Value
        {
        public:
            Value() = default;
            Value(const JsonTree* tree, u32 index) : m_tree(tree), m_index(index) {}

            explicit operator bool() const { return m_tree && type() != Type::Null; }
            Type type() const { return m_tree ? m_tree->m_nodes[m_index].type : Type::Null; }
            bool isObject() const { return type() == Type::Object; }
            bool isArray() const { return type() == Type::Array; }
            bool isString() const { return type() == Type::String; }
            bool isNumber() const { return type() == Type::Number; }

            Value operator[](const char* key) const;
            Value operator[](u32 index) const;
            Value first() const { return m_tree && m_tree->m_nodes[m_index].first ? Value(m_tree, m_tree->m_nodes[m_index].first) : Value(); }
            Value next() const { return m_tree && m_tree->m_nodes[m_index].next ? Value(m_tree, m_tree->m_nodes[m_index].next) : Value(); }
            // Unlike bool, tells a null that is there from one that isn't.
            bool exists() const { return m_tree != nullptr; }
            u32 count() const { return m_tree ? m_tree->m_nodes[m_index].count : 0; }
            u32 line() const { return m_tree ? m_tree->m_nodes[m_index].line : 0; }
            const char* key() const { return m_tree ? m_tree->m_text + m_tree->m_nodes[m_index].key : ""; }

            const char* text(const char* otherwise = "") const { return isString() ? m_tree->m_text + m_tree->m_nodes[m_index].text : otherwise; }
            float number(float otherwise = 0) const { return isNumber() ? static_cast<float>(m_tree->m_nodes[m_index].number) : otherwise; }
            double exact() const { return isNumber() ? m_tree->m_nodes[m_index].number : 0; }
            bool flag(bool otherwise = false) const { return type() == Type::Bool ? m_tree->m_nodes[m_index].flag : otherwise; }

        private:
            const JsonTree* m_tree = nullptr;
            u32 m_index = 0;
        };

        ~JsonTree() { memFree(m_text); }

        // Copies the text. False with the line and what was wrong if it isn't JSON.
        bool parse(const char* text, size_t size);
        Value root() const { return m_nodes.count() ? Value(this, 0) : Value(); }
        u32 errorLine() const { return m_line; }
        const char* error() const { return m_error; }

    private:
        static constexpr u32 kMaxDepth = 24;

        void space();
        bool fail(const char* what);
        bool value(u32 depth, u32& index);
        bool string(u32& offset);

        GrowArray<Node, MemTag::General> m_nodes;
        char* m_text = nullptr;
        char* m_at = nullptr;
        u32 m_line = 1;
        const char* m_error = "";
    };
}
