#pragma once

#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace html {
    /// bytes_to_encoded_characters converts bytes to a URL-encoded string.
    /// It is equivalent to JavaScript's btoa function.
    inline std::string bytes_to_encoded_characters(const std::vector<uint8_t>& bytes) {
        std::string output;
        output.reserve(bytes.size() * 4);
        const std::string characters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
        std::size_t data = 0;
        auto length = bytes.size();
        for (; length > 2; length -= 3) {
            data =
                ((static_cast<std::size_t>(bytes[bytes.size() - length]) << 16)
                 | (static_cast<std::size_t>(bytes[bytes.size() - length + 1]) << 8)
                 | bytes[bytes.size() - length + 2]);
            output.push_back(characters[(data & (63 << 18)) >> 18]);
            output.push_back(characters[(data & (63 << 12)) >> 12]);
            output.push_back(characters[(data & (63 << 6)) >> 6]);
            output.push_back(characters[data & 63]);
        }
        if (length == 2) {
            data = (static_cast<std::size_t>(bytes[bytes.size() - length]) << 16)
                   | (static_cast<std::size_t>(bytes[bytes.size() - length + 1]) << 8);
            output.push_back(characters[(data & (63 << 18)) >> 18]);
            output.push_back(characters[(data & (63 << 12)) >> 12]);
            output.push_back(characters[(data & (63 << 6)) >> 6]);
            output.push_back('=');
        } else if (length == 1) {
            data = (static_cast<std::size_t>(bytes[bytes.size() - length]) << 16);
            output.push_back(characters[(data & (63 << 18)) >> 18]);
            output.push_back(characters[(data & (63 << 12)) >> 12]);
            output.push_back('=');
            output.push_back('=');
        }
        return output;
    }

    /// variable stores either text or a boolean.
    class variable {
        public:
        variable(bool boolean) : _boolean(boolean), _is_boolean(true) {}
        variable(const char* text) : _boolean(false), _text(text), _is_boolean(false) {}
        variable(const std::string& text) : _boolean(false), _text(text), _is_boolean(false) {}
        variable(const variable&) = default;
        variable(variable&&) = default;
        variable& operator=(const variable&) = default;
        variable& operator=(variable&&) = default;
        virtual ~variable() {}

        /// to_boolean returns a boolean if the variable is a boolean, and throws an exception otherwise.
        virtual bool to_boolean() const {
            if (!_is_boolean) {
                throw std::logic_error("the variable is not a conditional");
            }
            return _boolean;
        }

        /// to_text returns a string if the variable is a text, and throws an exception otherwise.
        virtual const std::string& to_text() const {
            if (_is_boolean) {
                throw std::logic_error("the variable is not a text");
            }
            return _text;
        }

        /// is_boolean returns false if the variable is a string.
        virtual bool is_boolean() const {
            return _is_boolean;
        }

        protected:
        bool _boolean;
        std::string _text;
        bool _is_boolean;
    };

    /// node represents a part of an HTML template.
    class node {
        public:
        node(node* parent_node) : parent_node(parent_node) {}
        node(const node&) = delete;
        node(node&&) = default;
        node& operator=(const node&) = delete;
        node& operator=(node&&) = default;
        virtual ~node() {}

        /// parent_node is the node's enclosing node.
        node* parent_node;
    };

    /// text_node is a raw HTML text node.
    class text_node : public node {
        public:
        text_node(const std::string& content, node* parent_node) : node(parent_node), content(content) {}
        text_node(const text_node&) = delete;
        text_node(text_node&&) = default;
        text_node& operator=(const text_node&) = delete;
        text_node& operator=(text_node&&) = default;
        virtual ~text_node() {}

        /// content contains the node's text.
        const std::string content;
    };

    /// variable_node is a tag to be replaced.
    class variable_node : public node {
        public:
        variable_node(const std::string& name, node* parent_node) : node(parent_node), name(name) {}
        variable_node(const variable_node&) = delete;
        variable_node(variable_node&&) = default;
        variable_node& operator=(const variable_node&) = delete;
        variable_node& operator=(variable_node&&) = default;
        virtual ~variable_node() {}

        /// name is the variable's name.
        const std::string name;
    };

    /// conditional_node is a wrapper node which content is rendered conditionally.
    class conditional_node : public node {
        public:
        conditional_node(const std::string& name, node* parent_node, bool created_from_else_if, bool is_inline) :
            node(parent_node),
            name(name),
            created_from_else_if(created_from_else_if),
            is_inline(is_inline),
            in_else_block(false) {}
        conditional_node(const conditional_node&) = delete;
        conditional_node(conditional_node&&) = default;
        conditional_node& operator=(const conditional_node&) = delete;
        conditional_node& operator=(conditional_node&&) = default;
        virtual ~conditional_node() {}

        /// name is the variable's name.
        const std::string name;

        /// created_from_else_if is true if the node was created to represent an else-if statement.
        const bool created_from_else_if;

        /// is_inline is true if the node tags are on the same line, which impacts rendering.
        const bool is_inline;

        /// in_else_block is the current parser state.
        bool in_else_block;

        /// true_nodes returns the inner true nodes.
        std::vector<std::unique_ptr<node>>& true_nodes() {
            return _true_nodes;
        }

        /// ctrue_nodes returns a constant reference to the inner true nodes.
        const std::vector<std::unique_ptr<node>>& ctrue_nodes() const {
            return _true_nodes;
        }

        /// false_nodes returns the inner false nodes.
        std::vector<std::unique_ptr<node>>& false_nodes() {
            return _false_nodes;
        }

        /// cfalse_nodes returns a constant reference to the inner false nodes.
        const std::vector<std::unique_ptr<node>>& cfalse_nodes() const {
            return _false_nodes;
        }

        protected:
        std::vector<std::unique_ptr<node>> _true_nodes;
        std::vector<std::unique_ptr<node>> _false_nodes;
    };

    /// state holds the parser character state machine.
    enum class state {
        content,
        opening_brace,
        expression,
        closing_percent,
    };

    /// part represents either an expression or raw text.
    struct part {
        std::string content;
        bool is_expression;
        bool is_complete;
    };

    /// parse creates nodes from an HTML template.
    inline std::vector<std::unique_ptr<node>> parse(const std::string& html_template) {
        std::vector<std::unique_ptr<node>> nodes;
        conditional_node* parent_node = nullptr;
        auto current_nodes = std::ref(nodes);
        part previous_part{"", false, false};
        std::size_t line_count = 1;
        std::size_t character_count = 1;
        auto parse_error = [&](const std::string& message) {
            throw std::logic_error(
                "parse error: " + message + " (line " + std::to_string(line_count) + ":"
                + std::to_string(character_count) + ")");
        };
        std::istringstream stream(html_template);
        std::string line;
        while (std::getline(stream, line)) {
            auto state = state::content;
            std::vector<part> parts;
            if (!previous_part.content.empty() && previous_part.is_expression) {
                state = state::expression;
                parts.push_back(previous_part);
                previous_part.content.clear();
            }
            for (auto character_iterator = line.cbegin(); character_iterator != line.cend(); ++character_iterator) {
                switch (state) {
                    case state::content:
                        if (*character_iterator == '{') {
                            state = state::opening_brace;
                        } else {
                            if (parts.empty() || parts.back().is_expression) {
                                parts.push_back({"", false, false});
                            }
                            parts.back().content.push_back(*character_iterator);
                        }
                        break;
                    case state::opening_brace:
                        if (*character_iterator == '%') {
                            if (!parts.empty()) {
                                parts.back().is_complete = true;
                            }
                            parts.push_back(part{"", true, false});
                            state = state::expression;
                        } else {
                            if (parts.empty() || parts.back().is_expression) {
                                parts.push_back({"", false, false});
                            }
                            parts.back().content.push_back('{');
                            parts.back().content.push_back(*character_iterator);
                            state = state::content;
                        }
                        break;
                    case state::expression:
                        if (*character_iterator == '%') {
                            state = state::closing_percent;
                        } else if (
                            std::isspace(*character_iterator) || std::isalnum(*character_iterator)
                            || *character_iterator == '_') {
                            parts.back().content.push_back(*character_iterator);
                        } else {
                            parse_error("expected a word character or a percent sign");
                        }
                        break;
                    case state::closing_percent:
                        if (*character_iterator == '}') {
                            parts.back().is_complete = true;
                            state = state::content;
                        } else {
                            parse_error("expected a closing brace");
                        }
                        break;
                }
                ++character_count;
            }
            switch (state) {
                case state::content:
                    if (parts.empty() || parts.back().is_expression) {
                        parts.push_back({"", false, false});
                    }
                    parts.back().content.push_back('\n');
                    break;
                case state::opening_brace:
                    if (parts.empty() || parts.back().is_expression) {
                        parts.push_back({"", false, false});
                    }
                    parts.back().content.push_back('{');
                    parts.back().content.push_back('\n');
                    break;
                case state::expression:
                    parts.back().content.push_back('\n');
                    break;
                case state::closing_percent:
                    parse_error("expected a closing brace");
                    break;
            }
            auto is_inline = true;
            if (parts.size() == 1 && parts[0].is_expression && parts[0].is_complete) {
                is_inline = false;
            } else if (
                (parts.size() == 2 || parts.size() == 3) && !parts[0].is_expression
                && std::all_of(
                    parts[0].content.begin(),
                    parts[0].content.end(),
                    [](unsigned char character) { return std::isspace(character); })
                && parts[1].is_expression && parts[1].is_complete
                && (parts.size() == 2
                    || std::all_of(parts[2].content.begin(), parts[2].content.end(), [](unsigned char character) {
                           return std::isspace(character);
                       }))) {
                is_inline = false;
                parts = std::vector<part>{parts[1]};
            }
            if (!previous_part.content.empty()) {
                if (parts.front().is_expression) {
                    current_nodes.get().emplace_back(new text_node(previous_part.content, parent_node));
                } else {
                    previous_part.content.append(parts.front().content);
                    parts.front().content.swap(previous_part.content);
                }
                previous_part.content.clear();
            }
            if (!parts.back().is_complete) {
                previous_part = parts.back();
                parts.pop_back();
            }
            for (auto part : parts) {
                if (part.is_expression) {
                    std::vector<std::string> words;
                    {
                        auto next_word = true;
                        for (auto character : part.content) {
                            if (std::isspace(character)) {
                                next_word = true;
                            } else {
                                if (next_word) {
                                    next_word = false;
                                    words.emplace_back(1, character);
                                } else {
                                    words.back().push_back(character);
                                }
                            }
                        }
                    }
                    if (words.empty()) {
                        parse_error("empty tag");
                    } else if (words.size() == 1) {
                        if (words[0] == "else") {
                            if (parent_node == nullptr || parent_node->in_else_block) {
                                parse_error("unexpected 'else' tag");
                            } else {
                                if (!parent_node->is_inline && is_inline) {
                                    parse_error("the associated 'if' tag is not inline");
                                }
                                parent_node->in_else_block = true;
                                current_nodes = parent_node->false_nodes();
                            }
                        } else if (words[0] == "end") {
                            if (parent_node == nullptr) {
                                parse_error("unexpected 'end' tag");
                            } else {
                                for (;;) {
                                    const auto created_from_else_if = parent_node->created_from_else_if;
                                    parent_node = static_cast<conditional_node*>(parent_node->parent_node);
                                    if (parent_node == nullptr) {
                                        current_nodes = nodes;
                                        break;
                                    }
                                    if (!created_from_else_if) {
                                        if (parent_node->in_else_block) {
                                            current_nodes = parent_node->false_nodes();
                                        } else {
                                            current_nodes = parent_node->true_nodes();
                                        }
                                        break;
                                    }
                                    if (!parent_node->is_inline && is_inline) {
                                        parse_error("the associated 'if' tag is not inline");
                                    }
                                }
                            }
                        } else {
                            current_nodes.get().emplace_back(new variable_node(words[0], parent_node));
                        }
                    } else if (words.size() == 2) {
                        if (words[0] == "if") {
                            current_nodes.get().emplace_back(
                                new conditional_node(words[1], parent_node, false, is_inline));
                            parent_node = static_cast<conditional_node*>(current_nodes.get().back().get());
                            current_nodes = static_cast<conditional_node*>(parent_node)->true_nodes();
                        } else {
                            parse_error("expected 'if' as first word of a two-words tag");
                        }
                    } else if (words.size() == 3) {
                        if (words[0] == "else" && words[1] == "if") {
                            if (parent_node == nullptr || parent_node->in_else_block) {
                                parse_error("unexpected 'else if' tag");
                            } else {
                                if (!parent_node->is_inline && is_inline) {
                                    parse_error("the associated 'if' tag is not inline");
                                }
                                parent_node->in_else_block = true;
                                parent_node->false_nodes().emplace_back(
                                    new conditional_node(words[2], parent_node, true, is_inline));
                                parent_node = static_cast<conditional_node*>(parent_node->false_nodes().back().get());
                                current_nodes = static_cast<conditional_node*>(parent_node)->true_nodes();
                            }
                        } else {
                            parse_error("expected 'else if' as first words of a three-words tag");
                        }
                    } else {
                        parse_error("too many words in tag");
                    }
                } else {
                    current_nodes.get().emplace_back(new text_node(part.content, parent_node));
                }
            }
            ++line_count;
            character_count = 1;
        }
        if (parent_node != nullptr) {
            parse_error("expected 'end' tag");
        }
        if (!previous_part.content.empty()) {
            if (previous_part.is_expression) {
                parse_error("unclosed tag");
            } else {
                current_nodes.get().emplace_back(new text_node(previous_part.content, parent_node));
            }
        }
        return nodes;
    };

    /// render writes HTML from parsed nodes and variables.
    inline void render(
        std::ostream& output,
        const std::vector<std::unique_ptr<node>>& nodes,
        const std::unordered_map<std::string, variable>& name_to_variable,
        std::size_t indent = 0) {
        for (const auto& generic_node : nodes) {
            if (const auto node = dynamic_cast<const text_node*>(generic_node.get())) {
                if (indent == 0) {
                    output << node->content;
                } else {
                    std::size_t spaces_to_skip = indent * 4;
                    for (auto character : node->content) {
                        if (character == '\n') {
                            spaces_to_skip = indent * 4;
                        } else if (std::isspace(character) && spaces_to_skip > 0) {
                            --spaces_to_skip;
                            continue;
                        } else {
                            spaces_to_skip = 0;
                        }
                        output.put(character);
                    }
                }
            } else if (const auto node = dynamic_cast<const variable_node*>(generic_node.get())) {
                const auto name_and_variable = name_to_variable.find(node->name);
                if (name_and_variable == name_to_variable.end()) {
                    throw std::logic_error("unassigned variable '" + node->name + "'");
                }
                if (name_and_variable->second.is_boolean()) {
                    throw std::logic_error("a boolean is assigned to the text variable '" + node->name + "'");
                }
                output << name_and_variable->second.to_text();
            } else if (const auto node = dynamic_cast<const conditional_node*>(generic_node.get())) {
                const auto name_and_variable = name_to_variable.find(node->name);
                if (name_and_variable == name_to_variable.end()) {
                    throw std::logic_error("unassigned variable '" + node->name + "'");
                }
                if (!name_and_variable->second.is_boolean()) {
                    throw std::logic_error("a text is assigned to the boolean variable '" + node->name + "'");
                }
                if (name_and_variable->second.to_boolean()) {
                    render(
                        output,
                        node->ctrue_nodes(),
                        name_to_variable,
                        indent + (node->created_from_else_if || node->is_inline ? 0 : 1));
                } else {
                    render(
                        output,
                        node->cfalse_nodes(),
                        name_to_variable,
                        indent + (node->created_from_else_if || node->is_inline ? 0 : 1));
                }
            }
        }
    }
    inline void render(
        std::unique_ptr<std::ostream> output,
        const std::vector<std::unique_ptr<node>>& nodes,
        const std::unordered_map<std::string, variable>& name_to_variable,
        std::size_t indent = 0) {
        render(*output, nodes, name_to_variable, indent);
    }
}
