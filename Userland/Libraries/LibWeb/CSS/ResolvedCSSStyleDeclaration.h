/*
 * Copyright (c) 2021-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/CSS/CSSStyleDeclaration.h>

namespace Web::CSS {

class ResolvedCSSStyleDeclaration final : public CSSStyleDeclaration {
    WEB_PLATFORM_OBJECT(ResolvedCSSStyleDeclaration, CSSStyleDeclaration);

public:
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<ResolvedCSSStyleDeclaration>> create(DOM::Element& element);

    virtual ~ResolvedCSSStyleDeclaration() override = default;

    virtual size_t length() const override;
    virtual DeprecatedString item(size_t index) const override;
    virtual Optional<StyleProperty> property(PropertyID) const override;
    virtual WebIDL::ExceptionOr<void> set_property(PropertyID, StringView css_text, StringView priority) override;
    virtual WebIDL::ExceptionOr<DeprecatedString> remove_property(PropertyID) override;

    virtual DeprecatedString serialized() const override;
    virtual WebIDL::ExceptionOr<void> set_css_text(StringView) override;

private:
    explicit ResolvedCSSStyleDeclaration(DOM::Element&);

    virtual void visit_edges(Cell::Visitor&) override;

    ErrorOr<RefPtr<StyleValue const>> style_value_for_property(Layout::NodeWithStyle const&, PropertyID) const;

    JS::NonnullGCPtr<DOM::Element> m_element;
};

}
