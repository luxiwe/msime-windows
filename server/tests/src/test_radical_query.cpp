#include "tests/includes/test_framework.h"
#include "engine/contracts/radical_input.h"

#include <string>

// U 模式（部件拆字反查）：全拼 U、u 都进，双拼和五笔只认大写 U。形状规则与 TSF 共用一份。
TEST_CASE(radical_mode_composition_follows_the_scheme_trigger)
{
    using FanyImeRadicalInput::Trigger;
    const std::string upper = "Uririri";
    const std::string lower = "uririri";
    REQUIRE(FanyImeRadicalInput::IsComposition(upper.data(), upper.size(), Trigger::AnyCase));
    REQUIRE(FanyImeRadicalInput::IsComposition(lower.data(), lower.size(), Trigger::AnyCase));
    REQUIRE(FanyImeRadicalInput::IsComposition(upper.data(), upper.size(), Trigger::UppercaseOnly));
    REQUIRE(!FanyImeRadicalInput::IsComposition(lower.data(), lower.size(), Trigger::UppercaseOnly));
    REQUIRE(!FanyImeRadicalInput::IsComposition(upper.data(), upper.size(), Trigger::Off));
}

TEST_CASE(radical_mode_needs_a_second_letter)
{
    using FanyImeRadicalInput::Trigger;
    // 单独一个 u 或 U 仍是普通输入；nu / lu 这类音节里的 u 不在首位，也不会被认成 U 模式。
    const std::string single_upper = "U";
    const std::string single_lower = "u";
    const std::string syllable = "nu";
    const std::string empty;
    REQUIRE(!FanyImeRadicalInput::IsComposition(single_upper.data(), single_upper.size(), Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::IsComposition(single_lower.data(), single_lower.size(), Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::IsComposition(syllable.data(), syllable.size(), Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::IsComposition(empty.data(), empty.size(), Trigger::AnyCase));
    // 第二字符不是字母就不进，英文或标点开头的输入照旧。
    const std::string punctuation = "u'";
    const std::string digit = "u1";
    REQUIRE(!FanyImeRadicalInput::IsComposition(punctuation.data(), punctuation.size(), Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::IsComposition(digit.data(), digit.size(), Trigger::AnyCase));
}

TEST_CASE(radical_mode_keeps_letters_after_the_prefix)
{
    using FanyImeRadicalInput::Trigger;
    // 进了模式后混进的非字母仍算 U 模式，只是查不出候选：判据不要求后续全是 a-z，否则预编辑会在
    // 组合中途掉回拼音路径。
    const std::string mixed = "uir1";
    const std::string upper_mixed = "Uir1";
    REQUIRE(FanyImeRadicalInput::IsComposition(mixed.data(), mixed.size(), Trigger::AnyCase));
    REQUIRE(FanyImeRadicalInput::IsComposition(upper_mixed.data(), upper_mixed.size(), Trigger::UppercaseOnly));
}

TEST_CASE(radical_mode_accepts_letters_at_the_caret)
{
    using FanyImeRadicalInput::Trigger;
    const std::wstring typed = L"Uri";
    REQUIRE(FanyImeRadicalInput::AcceptsAt(typed.data(), typed.size(), typed.size(), L'x', Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::AcceptsAt(typed.data(), typed.size(), typed.size(), L'1', Trigger::AnyCase));
    REQUIRE(!FanyImeRadicalInput::AcceptsAt(typed.data(), typed.size(), 0, L'x', Trigger::AnyCase));
}
