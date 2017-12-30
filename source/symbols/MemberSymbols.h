//------------------------------------------------------------------------------
// MemberSymbols.h
// Contains member-related symbol definitions.
//
// File is under the MIT license; see LICENSE for details.
//------------------------------------------------------------------------------
#pragma once

#include <tuple>

#include "binding/ConstantValue.h"
#include "symbols/Definition.h"
#include "symbols/Lazy.h"
#include "symbols/SemanticFacts.h"
#include "symbols/StatementBodiedScope.h"
#include "symbols/Symbol.h"

namespace slang {

class PackageSymbol;

/// A class that wraps a hoisted transparent type member (such as an enum value)
/// into a parent scope. Whenever lookup finds one of these symbols, it will be
/// unwrapped into the underlying symbol instead.
class TransparentMemberSymbol : public Symbol {
public:
    const Symbol& wrapped;

    TransparentMemberSymbol(const Symbol& wrapped_) :
        Symbol(SymbolKind::TransparentMember, wrapped_.name, wrapped_.location),
        wrapped(wrapped_) {}
};

/// Represents an explicit import from a package. This symbol type is
/// special in that it won't be returned from a lookup() call; instead
/// it will be unwrapped into the imported symbol.
class ExplicitImportSymbol : public Symbol {
public:
    string_view packageName;
    string_view importName;

    ExplicitImportSymbol(string_view packageName, string_view importName, SourceLocation location) :
        Symbol(SymbolKind::ExplicitImport, importName, location),
        packageName(packageName), importName(importName) {}

    const PackageSymbol* package() const;
    const Symbol* importedSymbol() const;

private:
    mutable const PackageSymbol* package_ = nullptr;
    mutable const Symbol* import = nullptr;
    mutable bool initialized = false;
};

/// Represents a wildcard import declaration. This symbol is special in
/// that it won't be returned by a lookup, and won't even be in the name
/// map of a symbol at all. Instead there is a sideband list used to
/// resolve names via wildcard.
class WildcardImportSymbol : public Symbol {
public:
    string_view packageName;

    WildcardImportSymbol(string_view packageName, SourceLocation location) :
        Symbol(SymbolKind::WildcardImport, "", location),
        packageName(packageName) {}

    const PackageSymbol* getPackage() const;

private:
    mutable optional<const PackageSymbol*> package;
};

/// Represents a parameter value.
class ParameterSymbol : public Symbol {
public:
    ParameterSymbol(string_view name, SourceLocation loc, bool isLocal_, bool isPort_) :
        Symbol(SymbolKind::Parameter, name, loc),
        type(this), isLocal(isLocal_), isPort(isPort_) {}

    static void fromSyntax(Compilation& compilation, const ParameterDeclarationSyntax& syntax,
                           SmallVector<ParameterSymbol*>& results);

    static ParameterSymbol& fromDecl(Compilation& compilation, const Definition::ParameterDecl& decl);

    static std::tuple<const Type*, ConstantValue> evaluate(const DataTypeSyntax& type,
                                                           const ExpressionSyntax& expr, const Scope& scope);

    void setDeclaredType(const DataTypeSyntax& syntax) { declaredType = &syntax; type = syntax; }
    const DataTypeSyntax* getDeclaredType() const { return declaredType; }

    const Type& getType() const;
    void setType(const Type& newType) { type = newType; }

    const LazyType& getLazyType() const { return type; }

    const ConstantValue& getValue() const;
    void setValue(ConstantValue value);

    const ConstantValue* getDefault() const;
    void setDefault(ConstantValue&& value);
    void setDefault(const ExpressionSyntax& syntax);

    bool hasDefault() const { return (bool)defaultValue; }
    bool isLocalParam() const { return isLocal; }
    bool isPortParam() const { return isPort; }
    bool isBodyParam() const { return !isPortParam(); }

private:
    const DataTypeSyntax* declaredType = nullptr;
    mutable LazyType type;
    mutable const ConstantValue* value = nullptr;
    mutable PointerUnion<const ConstantValue*, const ExpressionSyntax*> defaultValue;
    bool isLocal = false;
    bool isPort = false;
};

/// Represents a variable declaration (which does not include nets).
class VariableSymbol : public Symbol {
public:
    LazyType type;
    LazyInitializer initializer;
    VariableLifetime lifetime;
    bool isConst;

    VariableSymbol(string_view name, SourceLocation loc,
                   VariableLifetime lifetime = VariableLifetime::Automatic, bool isConst = false) :
        VariableSymbol(SymbolKind::Variable, name, loc, lifetime, isConst) {}

    /// Constructs all variable symbols specified by the given syntax node.
    static void fromSyntax(Compilation& compilation, const DataDeclarationSyntax& syntax,
                           SmallVector<const VariableSymbol*>& results);

    static VariableSymbol& fromSyntax(Compilation& compilation, const ForVariableDeclarationSyntax& syntax);

protected:
    VariableSymbol(SymbolKind childKind, string_view name, SourceLocation loc,
                   VariableLifetime lifetime = VariableLifetime::Automatic, bool isConst = false) :
        Symbol(childKind, name, loc),
        type(this),
        initializer(this),
        lifetime(lifetime),
        isConst(isConst) {}
};

/// Represents a formal argument in subroutine (task or function).
class FormalArgumentSymbol : public VariableSymbol {
public:
    FormalArgumentDirection direction = FormalArgumentDirection::In;

    FormalArgumentSymbol() :
        VariableSymbol(SymbolKind::FormalArgument, "", SourceLocation()) {}

    FormalArgumentSymbol(string_view name, SourceLocation loc,
                         FormalArgumentDirection direction = FormalArgumentDirection::In) :
        VariableSymbol(SymbolKind::FormalArgument, name, loc, VariableLifetime::Automatic,
                       direction == FormalArgumentDirection::ConstRef),
        direction(direction) {}
};

/// Represents a subroutine (task or function).
class SubroutineSymbol : public Symbol, public StatementBodiedScope {
public:
    using ArgList = span<const FormalArgumentSymbol* const>;

    LazyType returnType;
    ArgList arguments;
    VariableLifetime defaultLifetime = VariableLifetime::Automatic;
    SystemFunction systemFunctionKind = SystemFunction::Unknown;
    bool isTask = false;

    SubroutineSymbol(Compilation& compilation, string_view name, SourceLocation loc,
                     VariableLifetime defaultLifetime, bool isTask) :
        Symbol(SymbolKind::Subroutine, name, loc),
        StatementBodiedScope(compilation, this),
        returnType(static_cast<const Scope*>(this)),
        defaultLifetime(defaultLifetime), isTask(isTask) {}

    SubroutineSymbol(Compilation& compilation, string_view name, SourceLocation loc,
                     SystemFunction systemFunction) :
        Symbol(SymbolKind::Subroutine, name, loc),
        StatementBodiedScope(compilation, this),
        returnType(static_cast<const Scope*>(this)),
        systemFunctionKind(systemFunction) {}

    static SubroutineSymbol& fromSyntax(Compilation& compilation, const FunctionDeclarationSyntax& syntax);

    bool isSystemFunction() const { return systemFunctionKind != SystemFunction::Unknown; }
};

}