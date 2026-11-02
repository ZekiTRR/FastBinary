#pragma once

#include <vector>
#include <string>
#include <optional>
#include <expected>

#include "WINAPI/TYPES.h"

class Memory {

public:

    // Объект создаётся при бросании файла в Qt: путь достаётся из дропа
    // и сразу передаётся в конструктор, который сразу же загружает файл.
    explicit Memory(const std::wstring& path) {
        if (auto result = loadFile(path); !result) {
            load_error_ = std::move(result.error());
        }
    }


private:

    std::vector<unsigned char> buffer_;
    std::optional<std::string> load_error_;


public:


    // Загрузка файла в буффер (вектор)
    std::expected<void, std::string> loadFile(const std::wstring& path);

    // Загружен ли файл (конструктор грузит сразу - здесь результат этой загрузки)
    [[nodiscard]]
    bool isLoaded() const noexcept {
        return !load_error_.has_value();
    }

    // Причина неудачной загрузки (пустая, если isLoaded() == true)
    [[nodiscard]]
    const std::optional<std::string>& loadError() const noexcept {
        return load_error_;
    }

    // Возвращает ссылку на буффер
    [[nodiscard]]
    const std::vector<unsigned char>& getBuffer() const noexcept {
        return buffer_;
    }

    // Поиск паттерна в буффере: возвращает оффсет (позицию) от начала,
    [[nodiscard]]
    std::optional<QWORD> pos_pattern_find(const std::vector<UINT>& pattern) const;

};
