#pragma once

#include <cstddef>

// U 模式（部件拆字反查）的输入形状。全拼、双拼、五笔都从 U 或 u 开始，其后跟 a-z 字母就是拆字输入：
//
//   ririri            晶 = 日 + 日 + 日
//   shikai            研 = 石 + 开
//
// 全拼音节表里没有 u 和 v 开头的音节（u 只可能出现在声母之后，如 nu / lu；v 只是 ü 的占位写法，
// 如 jv / nv），所以全拼默认放开小写 u 不会抢走正常拼音；双拼的 u 是韵母键、五笔的 u 是字根键，两者
// 只认大写 U。这一划分与 V 模式（v_mode_input.h）同构，见 Trigger。全拼的小写 u 可以用
// utility.radical_mode_lowercase 关掉，关掉后全拼也只认大写 U。
//
// 和 V 模式不同，第二字符是字母而不是数字，所以 u 之后任何字母都算进入了这个模式，不看后续字母
// 是否在拆字表里查得到。查不到只是没有候选，判据不去查表：中途按查表结果进出模式会让预编辑在
// 组合中途变形（K/T/E/M/S/J/Y 那批模式踩过这个坑）。
//
// 只有 Server 用这个头判断形状。TSF 不需要它：U 模式的字母本来就是普通编码键，没有要吃的键，
// 所以这个 Trigger 不跨进程同步，也没有对应的 worker 消息。
namespace FanyImeRadicalInput
{
inline constexpr char kUpperPrefix = 'U';
inline constexpr char kLowerPrefix = 'u';

// 当前方案认哪个前缀。这是 Server 内部的值，不是 IPC 载荷。
enum class Trigger
{
    Off = 0,           // 关着，或者其他方案（日文、专用英文等）
    UppercaseOnly = 1, // 双拼、五笔：只认大写 U
    AnyCase = 2,       // 全拼：U 和 u 都认
};

template <typename Char> constexpr bool IsLetter(Char ch)
{
    return (ch >= Char('a') && ch <= Char('z')) || (ch >= Char('A') && ch <= Char('Z'));
}

template <typename Char> constexpr bool IsPrefix(Char ch, Trigger trigger)
{
    return (trigger != Trigger::Off && ch == Char(kUpperPrefix)) ||
           (trigger == Trigger::AnyCase && ch == Char(kLowerPrefix));
}

// text 是不是一段 U 模式输入：开头是 U（或全拼的 u），后面紧跟一个字母。单独一个 U / u 还不算，
// 它照旧是普通的组合（英文、拼音的开头），接上第一个字母才进这个模式。之后混进 a-z 以外的键仍算
// U 模式，只是没有候选。
template <typename Char> constexpr bool IsComposition(const Char *text, std::size_t size, Trigger trigger)
{
    return text != nullptr && size >= 2 && IsPrefix(text[0], trigger) && IsLetter(text[1]);
}

// 光标停在 text[caret] 时能否插入 ch：ch 是字母，插入后整串仍是一段 U 模式输入。所以 U 后面第一个
// 键只能是一个字母。
//
// NOTE: 当前没有产品代码调用它。U 模式的字母本来就是普通编码键，TSF 不需要为它吃任何键，所以不像
// V 模式的 AcceptsAt 那样有消费方（V 模式要吃掉数字和运算符）。保留它是为了让形状规则与 V 模式
// 对称、并给将来的分隔符语法留入口；只有测试在调用。真要接上之前先确认 TSF 侧确实需要吃键，
// 否则又会多出一条只写不读的死代码。
template <typename Char>
constexpr bool AcceptsAt(const Char *text, std::size_t size, std::size_t caret, Char ch, Trigger trigger)
{
    if (text == nullptr || size == 0 || caret < 1 || caret > size || !IsLetter(ch) || !IsPrefix(text[0], trigger))
    {
        return false;
    }
    const Char second = caret == 1 ? ch : text[1];
    return IsLetter(second);
}
} // namespace FanyImeRadicalInput
