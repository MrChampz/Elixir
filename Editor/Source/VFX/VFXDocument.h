#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace EditorVFX
{
    /** @brief Identifies the JSON value stored in an editable effect. */
    enum class EValueType { Null, Boolean, Number, String, Array, Object };

    /** @brief Owns one JSON value, including unknown asset fields for lossless editing. */
    struct Value
    {
        EValueType Type = EValueType::Null;
        /** @brief Decoded string or the original JSON scalar token. */
        std::string Text;
        std::vector<Value> Array;
        std::map<std::string, Value> Object;

        /** @brief Finds an object member, or returns null when absent. */
        const Value* Find(const std::string& key) const;
        /** @brief Returns a string member or its fallback. */
        std::string String(const std::string& key, const std::string& fallback = "") const;
        /** @brief Returns the optional authoring toggle; absent toggles are enabled. */
        bool Enabled() const;
        /** @brief Serializes the value as valid JSON. */
        std::string JSON() const;
        /** @brief Parses a JSON value without exceptions. */
        static bool Parse(const std::string& text, Value& result, std::string& error);
        /** @brief Creates a decoded JSON string. */
        static Value FromString(std::string text);
    };

    using Path = std::vector<std::string>;

    /** @brief Stores a property edit; array components use decimal indices in its path. */
    struct Edit { Path Location; std::string Text; };

    /** @brief Edits an effect document with per-asset history and conflict-aware file saving. */
    class Document
    {
    public:
        /** @brief Reads an asset and clears its editing history on success. */
        bool Load(const std::filesystem::path& path, std::string& error);
        /** @brief Creates an unsaved document from JSON and associates it with a new asset path. */
        bool Create(const std::filesystem::path& path, const std::string& json, std::string& error);
        /** @brief Saves changes using a temporary file; refuses to overwrite external edits. */
        bool Save(std::string& error);
        /** @brief Returns the current asset definition. */
        const Value& Root() const { return m_Root; }
        /** @brief Returns the asset's destination path. */
        const std::filesystem::path& FilePath() const { return m_Path; }
        /** @brief Finds a property using member names and array indices. */
        const Value* Get(const Path& path) const;
        /** @brief Applies property edits as one undo step; invalid input leaves the document unchanged. */
        bool Apply(const std::vector<Edit>& edits, std::string& error);
        /** @brief Replaces a property and records one undo step. */
        bool Set(const Path& path, Value value);
        /** @brief Appends an element to an array and records one undo step. */
        bool Append(const Path& path, Value value);
        /** @brief Removes an array element and records one undo step. */
        bool Remove(const Path& path, size_t index);
        /** @brief Moves an array element by one slot; boundary moves are ignored. */
        bool Move(const Path& path, size_t index, int direction);
        /** @brief Restores the previous definition. */
        bool Undo();
        /** @brief Restores the next definition. */
        bool Redo();
        /** @brief Returns whether the serialized definition differs from the last saved version. */
        bool Dirty() const { return m_Root.JSON() != m_Saved; }
        /** @brief Returns whether an undo step is available. */
        bool CanUndo() const { return !m_Undo.empty(); }
        /** @brief Returns whether a redo step is available. */
        bool CanRedo() const { return !m_Redo.empty(); }

    private:
        // Records the previous definition and clears redo history.
        void Commit(Value root);
        Value m_Root;
        std::filesystem::path m_Path;
        std::string m_Saved;
        std::string m_DiskText;
        bool m_ExistsOnDisk = false;
        std::vector<Value> m_Undo;
        std::vector<Value> m_Redo;
    };

    /** @brief Returns a usable sprite emitter with default spawn and update modules. */
    Value MakeEmitter(const std::string& name);
    /** @brief Returns default module definitions supported by the selected phase. */
    std::vector<Value> ModuleTemplates(bool spawn);
}
