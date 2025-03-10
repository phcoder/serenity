/*
 * Copyright (c) 2023, Ali Mohammad Pur <mpfard@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <LibWeb/CSS/CSSRule.h>
#include <LibWeb/CSS/CSSStyleDeclaration.h>
#include <LibWeb/CSS/Percentage.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::CSS {

// https://drafts.csswg.org/css-animations/#interface-csskeyframerule
class CSSKeyframeRule final : public CSSRule {
    WEB_PLATFORM_OBJECT(CSSKeyframeRule, CSSRule);

public:
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<CSSKeyframeRule>> create(JS::Realm& realm, CSS::Percentage key, CSSStyleDeclaration& declarations);

    virtual ~CSSKeyframeRule() = default;

    virtual Type type() const override { return Type::Keyframe; }

    CSS::Percentage key() const { return m_key; }
    JS::NonnullGCPtr<CSSStyleDeclaration> style() const { return m_declarations; }

    DeprecatedString key_text() const
    {
        return m_key.to_string().release_value_but_fixme_should_propagate_errors().to_deprecated_string();
    }

    void set_key_text(DeprecatedString const& key_text)
    {
        dbgln("FIXME: CSSKeyframeRule::set_key_text is not implemented: {}", key_text);
    }

private:
    CSSKeyframeRule(JS::Realm& realm, CSS::Percentage key, CSSStyleDeclaration& declarations)
        : CSSRule(realm)
        , m_key(key)
        , m_declarations(declarations)
    {
    }

    virtual void visit_edges(Visitor&) override;
    virtual JS::ThrowCompletionOr<void> initialize(JS::Realm&) override;
    virtual DeprecatedString serialized() const override;

    CSS::Percentage m_key;
    JS::NonnullGCPtr<CSSStyleDeclaration> m_declarations;
};

template<>
inline bool CSSRule::fast_is<CSSKeyframeRule>() const { return type() == CSSRule::Type::Keyframe; }

}
