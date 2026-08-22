# Estilos visuais por estado para componentes GUI

## 1. Objetivo

Substituir a API fragmentada de propriedades visuais por estado, por exemplo:

```cpp
void SetNormalColor(const SColor& color);
void SetHoverColor(const SColor& color);
void SetNormalBackground(const Ref<Texture2D>& texture);
```

por um modelo único, extensível e determinístico. O modelo deve atender a
`Normal`, `Hovered`, `Pressed` e `Disabled` desde a primeira implementação,
com a ordem de composição obrigatória:

```text
Normal < Hovered < Pressed < Disabled
```

Em outras palavras, quando mais de um estado estiver ativo, a camada à direita
vence para cada propriedade que ela declarar. `Disabled` sempre vence;
`Pressed` vence `Hovered`; e `Hovered` vence `Normal`.

O foco deste documento é **aparência**. Estados de interação continuam sendo
responsabilidade de `Widget` e do roteamento de input; o resolvedor apenas lê
esses estados e produz o estilo que será desenhado no frame atual.

## 2. Contexto e problema atual

`Button` já alterna entre `m_NormalColor` e `m_HoverColor` em
`BuildDrawCommands`, enquanto mantém uma única `m_NormalBackground`. Essa
representação escala mal: cada nova propriedade ou estado cria mais campos,
getters, setters e condicionais no desenho (`SetPressedColor`,
`SetDisabledBackground`, `SetFocusedOutline`, e assim por diante).

Além disso, uma seleção simples com `if/else` não descreve corretamente
combinações reais. Um botão pressionado normalmente continua sob o cursor; um
componente desabilitado pode continuar hovered até o cursor sair. A aparência
precisa de uma regra explícita para esses casos.

## 3. Decisões de design

### 3.1 Separar estado de interação e estilo visual

`EUIInteractionState` descreve fatos transitórios ou semânticos do widget. Ele
não armazena cores, texturas ou geometria.

`SUIStyleOverride` descreve apenas diferenças visuais. Ele não altera o
comportamento do widget, não muda foco e não decide se um clique é aceito.

Essa separação evita que uma API de aparência se transforme em uma máquina de
estados de input e mantém a origem de `Hovered`, `Pressed` e `Focused` no
`Widget`, onde ela já existe.

### 3.2 Estilos são overrides, não cópias completas

Todo campo de `SUIStyleOverride` é opcional:

* campo ausente: herdar o valor já resolvido da camada anterior;
* campo presente: substituir o valor resolvido até aquele ponto;
* `BackgroundTexture = Ref<Texture2D>{}`: remover explicitamente uma textura
  herdada e permitir que o renderer desenhe o fundo sólido.

O último ponto exige `std::optional<Ref<Texture2D>>`, e não somente
`Ref<Texture2D>`. Um `Ref` nulo sozinho não distingue “não configurei este
estado” de “quero limpar a textura herdada”.

`Normal` é a base e deve declarar um valor efetivo para toda propriedade que o
widget precisa para desenhar. Os estados seguintes podem declarar somente o
que diverge.

### 3.3 Estados iniciais e extensibilidade

O armazenamento de estilos usa um `enum` fechado, indexado por `std::array`.
Isto evita alocação e mantém a cobertura dos estados visível em revisão. O
estado de interação é uma máscara de bits porque `Hovered` e `Pressed` podem
estar ativos simultaneamente.

`Focused` e `Selected` não fazem parte da prioridade inicial solicitada. Quando
forem necessários, devem ser introduzidos conscientemente como uma camada de
estilo ou como uma decoração independente (por exemplo, um focus ring). Não
devem ser adicionados de modo implícito a uma precedência existente.

### 3.4 Comentários e documentação pública

O código desta implementação deve seguir linguagem simples, conforme os
princípios da ISO 24495-1:2023. Isso significa escrever para o leitor que vai
manter o código: usar frases curtas, voz direta, termos consistentes e a
terminologia do domínio (`layer`, `override`, `resolved style` e `disabled`)
sem sinônimos desnecessários.

Comentários de implementação são permitidos somente quando forem estritamente
necessários para explicar algo que o código, os nomes e a assinatura não tornam
claro. Casos típicos aceitos são:

* uma restrição ou decisão de design que evita uma regressão;
* a razão de uma ordem que parece contraintuitiva;
* uma limitação de ciclo de vida, ownership ou API externa.

Não comentar o óbvio, repetir nomes, narrar atribuições ou usar comentários
como substituto para nomes claros e métodos pequenos. Por exemplo, não usar
`// Apply hovered style` imediatamente antes de uma chamada autoexplicativa a
`ApplyOverride`; o nome e a estrutura do resolvedor já comunicam isso.

Todo método público novo ou alterado deve ter documentação curta de contrato,
no formato Doxygen já usado no projeto. A documentação deve dizer o que o
método faz, e incluir `@param`, `@return` ou efeitos relevantes apenas quando
isso ajudar a usar o método corretamente. Ela deve declarar especialmente:

* `SetStyle`: substitui o override completo da layer e marca a renderização
  como dirty;
* `ClearStyle`: remove todos os overrides da layer, restaurando o fallback;
* setters de propriedade: definem somente aquela propriedade na layer;
* `ClearBackgroundTexture`: remove explicitamente uma textura herdada;
* `Resolve`: retorna o estilo composto, aplicando a precedência definida neste
  documento;
* `SetEnabled`: altera a disponibilidade de interação, não a visibilidade nem
  o layout.

Exemplo de documentação adequada:

```cpp
/**
 * Set one visual property for a style layer.
 * @param layer Layer that owns the override.
 * @param color Background color for that layer.
 */
void SetBackgroundColor(EUIVisualLayer layer, const SColor& color);
```

O comentário não deve repetir detalhes que pertencem ao nome da função. Não
usar documentação longa em métodos simples; as regras de prioridade e o motivo
da composição pertencem a este documento e aos testes, não a cópias divergentes
em cada header.

## 4. API proposta

### 4.1 Tipos compartilhados

Os tipos devem ficar em um header compartilhado de GUI, por exemplo
`Engine/GUI/UIStyle.h`. Os includes exatos devem seguir os tipos que hoje
declaram `SColor`, `SOutline` e `Texture2D`.

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace Elixir::GUI
{
    enum class EUIVisualLayer : uint8_t
    {
        Normal,
        Hovered,
        Pressed,
        Disabled,
        Count,
    };

    enum class EUIInteractionState : uint8_t
    {
        None     = 0,
        Hovered  = 1 << 0,
        Pressed  = 1 << 1,
        Disabled = 1 << 2,
    };

    constexpr EUIInteractionState operator|(
        EUIInteractionState left,
        EUIInteractionState right)
    {
        return static_cast<EUIInteractionState>(
            static_cast<uint8_t>(left) | static_cast<uint8_t>(right)
        );
    }

    constexpr bool HasState(
        EUIInteractionState states,
        EUIInteractionState state)
    {
        return (static_cast<uint8_t>(states) & static_cast<uint8_t>(state)) != 0;
    }

    struct SUIStyleOverride
    {
        std::optional<SColor> BackgroundColor;
        std::optional<SColor> ForegroundColor;
        std::optional<Ref<Texture2D>> BackgroundTexture;
        std::optional<glm::vec4> BackgroundBorders;
        std::optional<glm::vec4> CornerRadius;
        std::optional<SOutline> Outline;
        std::optional<glm::vec4> InsetShadow;
        std::optional<glm::vec4> DropShadow;
    };

    // Estilo pronto para ser consumido por BuildDrawCommands: nenhum campo é opcional.
    struct SResolvedUIStyle
    {
        SColor BackgroundColor;
        SColor ForegroundColor;
        Ref<Texture2D> BackgroundTexture;
        glm::vec4 BackgroundBorders;
        glm::vec4 CornerRadius;
        SOutline Outline;
        glm::vec4 InsetShadow;
        glm::vec4 DropShadow;
    };

    class ELIXIR_API UIStyleSet
    {
      public:
        const SUIStyleOverride& Get(EUIVisualLayer layer) const;
        void Set(EUIVisualLayer layer, const SUIStyleOverride& style);
        void Clear(EUIVisualLayer layer);

        SResolvedUIStyle Resolve(EUIInteractionState states) const;

      private:
        std::array<SUIStyleOverride, static_cast<size_t>(EUIVisualLayer::Count)>
            m_Layers;
    };
}
```

`EUIVisualLayer` não é uma máscara: cada valor representa uma camada de
estilo editável. `EUIInteractionState` é uma máscara: é a fotografia dos
estados ativos no instante da renderização.

### 4.2 API pública dos componentes

Um componente que suporta esse sistema expõe uma API genérica e curta:

```cpp
class ELIXIR_API Button : public ContentWidget
{
  public:
    const SUIStyleOverride& GetStyle(EUIVisualLayer layer) const;
    void SetStyle(EUIVisualLayer layer, const SUIStyleOverride& style);
    void ClearStyle(EUIVisualLayer layer);

    void SetBackgroundColor(EUIVisualLayer layer, const SColor& color);
    void SetForegroundColor(EUIVisualLayer layer, const SColor& color);
    void SetBackgroundTexture(EUIVisualLayer layer, const Ref<Texture2D>& texture);
    void ClearBackgroundTexture(EUIVisualLayer layer);

  protected:
    EUIInteractionState GetInteractionStates() const;
    const SResolvedUIStyle& GetResolvedStyle() const;

  private:
    UIStyleSet m_Styles;
    mutable SResolvedUIStyle m_ResolvedStyle;
    mutable bool m_StyleResolutionDirty = true;
    bool m_Enabled = true;
};
```

Os setters de conveniência são opcionais, mas devem continuar parametrizados
pela camada. Eles tornam os usos frequentes legíveis sem recriar uma API por
estado:

```cpp
button.SetBackgroundColor(EUIVisualLayer::Normal,  { 0.3f, 0.3f, 0.8f, 1.0f });
button.SetBackgroundColor(EUIVisualLayer::Hovered, { 0.4f, 0.4f, 0.9f, 1.0f });
button.SetBackgroundColor(EUIVisualLayer::Pressed, { 0.2f, 0.2f, 0.6f, 1.0f });
button.SetBackgroundColor(EUIVisualLayer::Disabled,{ 0.2f, 0.2f, 0.2f, 1.0f });
```

Não introduzir `SetNormalColor`, `SetHoverColor`, `SetPressedColor` ou
equivalentes novos. Os existentes podem ser removidos na migração completa ou
mantidos temporariamente como wrappers descontinuados para o novo método.

### 4.3 `Disabled` é estado semântico, não visual somente

`Widget` atualmente já possui `m_Hovered`, `m_Pressed` e `m_Focused`, mas não
um estado habilitado. A implementação deve introduzir, na classe apropriada da
hierarquia, pelo menos:

```cpp
bool IsEnabled() const { return m_Enabled; }
void SetEnabled(bool enabled);
```

`SetEnabled(false)` deve:

1. marcar a renderização como dirty;
2. impedir novos mouse-downs e ativações/clicks do componente;
3. cancelar ou ignorar uma ativação pendente iniciada antes da desabilitação;
4. não mudar `EVisibility` nem a participação do widget no layout.

O estado visual `Disabled` ganha a prioridade máxima independentemente de
`m_Hovered` ou `m_Pressed` ainda refletirem um evento processado no mesmo
frame. A normalização da interação pode limpar `Pressed` ao desabilitar, mas o
resolvedor não depende dessa limpeza para ser correto.

## 5. Resolvedor de layers

### 5.1 Contrato

`Resolve` começa com a camada `Normal` e aplica, nessa ordem, as layers que
estão ativas:

```text
Normal -> Hovered -> Pressed -> Disabled
```

Uma layer inativa não participa. Para cada campo, a última layer ativa que
declara aquele campo vence. Assim, uma camada `Pressed` que muda somente a cor
preserva a textura configurada em `Hovered` ou `Normal`; `Disabled` pode trocar
somente `ForegroundColor` e ainda assim manter o restante já composto.

### 5.2 Pseudocódigo de referência

```cpp
namespace
{
    constexpr size_t ToIndex(EUIVisualLayer layer)
    {
        return static_cast<size_t>(layer);
    }

    void ApplyOverride(
        SResolvedUIStyle& destination,
        const SUIStyleOverride& override)
    {
        if (override.BackgroundColor)
            destination.BackgroundColor = *override.BackgroundColor;
        if (override.ForegroundColor)
            destination.ForegroundColor = *override.ForegroundColor;
        if (override.BackgroundTexture)
            destination.BackgroundTexture = *override.BackgroundTexture;
        if (override.BackgroundBorders)
            destination.BackgroundBorders = *override.BackgroundBorders;
        if (override.CornerRadius)
            destination.CornerRadius = *override.CornerRadius;
        if (override.Outline)
            destination.Outline = *override.Outline;
        if (override.InsetShadow)
            destination.InsetShadow = *override.InsetShadow;
        if (override.DropShadow)
            destination.DropShadow = *override.DropShadow;
    }
}

SResolvedUIStyle UIStyleSet::Resolve(EUIInteractionState states) const
{
    SResolvedUIStyle result{};

    // Normal deve preencher todos os campos necessários para desenhar.
    ApplyOverride(result, m_Layers[ToIndex(EUIVisualLayer::Normal)]);

    if (HasState(states, EUIInteractionState::Hovered))
        ApplyOverride(result, m_Layers[ToIndex(EUIVisualLayer::Hovered)]);

    if (HasState(states, EUIInteractionState::Pressed))
        ApplyOverride(result, m_Layers[ToIndex(EUIVisualLayer::Pressed)]);

    if (HasState(states, EUIInteractionState::Disabled))
        ApplyOverride(result, m_Layers[ToIndex(EUIVisualLayer::Disabled)]);

    return result;
}
```

Na implementação real, `Normal` deve ser validado antes de renderizar. Há duas
alternativas aceitáveis:

* inicializar os valores de `Normal` com os defaults atuais do componente;
* manter defaults completos no construtor de `SResolvedUIStyle` e tratar
  `Normal` como override sobre esses defaults.

A primeira alternativa é preferida para a migração de `Button`, pois preserva
exatamente os valores atuais no ponto em que hoje os campos são declarados.

### 5.3 Montagem da máscara pelo componente

```cpp
EUIInteractionState Button::GetInteractionStates() const
{
    EUIInteractionState states = EUIInteractionState::None;

    if (IsHovered())
        states = states | EUIInteractionState::Hovered;
    if (IsPressed())
        states = states | EUIInteractionState::Pressed;
    if (!IsEnabled())
        states = states | EUIInteractionState::Disabled;

    return states;
}
```

`BuildDrawCommands` obtém o resultado uma vez e usa somente ele:

```cpp
const SResolvedUIStyle& style = GetResolvedStyle();

if (style.BackgroundTexture)
{
    batch.AddTexture(
        style.BackgroundTexture,
        m_Geometry,
        style.BackgroundBorders,
        style.BackgroundColor,
        zOrder
    );
}
else
{
    batch.AddRect(
        m_Geometry,
        style.BackgroundColor,
        style.CornerRadius,
        style.InsetShadow,
        style.DropShadow,
        style.Outline,
        zOrder
    );
}

// Quando o Button desenhar texto próprio:
batch.AddText(/* ... */, style.ForegroundColor, zOrder + 1, m_Geometry);
```

O método não deve consultar `m_Hovered`, `m_Pressed` nem `m_Enabled` para
escolher propriedades individualmente depois de resolver o estilo. Isso
centraliza a precedência em um único lugar.

## 6. Cache, invalidação e custo

`GetResolvedStyle()` pode recalcular a cada chamada sem impacto relevante com
quatro layers e poucos campos. Ainda assim, a API deve permitir cache local:

```cpp
const SResolvedUIStyle& Button::GetResolvedStyle() const
{
    if (m_StyleResolutionDirty)
    {
        m_ResolvedStyle = m_Styles.Resolve(GetInteractionStates());
        m_StyleResolutionDirty = false;
    }

    return m_ResolvedStyle;
}
```

Para que esse cache seja correto, marcar `m_StyleResolutionDirty = true` e
chamar `MarkRenderDirty()` quando ocorrer qualquer um destes eventos:

* `SetStyle`, `ClearStyle` ou qualquer setter de conveniência;
* entrada ou saída de hover;
* início ou fim de press;
* `SetEnabled`;
* qualquer futuro estado incluído na máscara.

Como `Widget` já marca renderização como dirty em entrada/saída de mouse, foco
e mouse-down/up, a primeira integração pode simplesmente resolver dentro de
`BuildDrawCommands` sem cache. O cache só deve ser adicionado se o estilo for
consultado mais de uma vez por frame ou se a medição mostrar necessidade; se
adicionado, todos os caminhos acima precisam invalidá-lo.

## 7. Migração de `Button`

### 7.1 Mapeamento dos dados atuais

| Campo atual | Novo destino |
| --- | --- |
| `m_NormalColor` | `m_Styles[Normal].BackgroundColor` |
| `m_HoverColor` | `m_Styles[Hovered].BackgroundColor` |
| `m_NormalBackground` | `m_Styles[Normal].BackgroundTexture` |
| `m_BackgroundBorders` | `m_Styles[Normal].BackgroundBorders` |
| `m_CornerRadius` | `m_Styles[Normal].CornerRadius` |
| `m_TextColor` | `m_Styles[Normal].ForegroundColor` |
| `m_Outline`, sombras herdadas | inicialmente `Normal`; depois configuráveis por layer conforme necessário |

Os defaults atuais de `Button` devem ser registrados em `Normal` no construtor
ou como inicializadores de `UIStyleSet`, mantendo a aparência existente quando
nenhuma camada nova é configurada.

### 7.2 Compatibilidade temporária

Se for importante migrar call sites em mais de um diff, os setters antigos
podem sobreviver temporariamente como wrappers:

```cpp
void Button::SetNormalColor(const SColor& color)
{
    SetBackgroundColor(EUIVisualLayer::Normal, color);
}

void Button::SetHoverColor(const SColor& color)
{
    SetBackgroundColor(EUIVisualLayer::Hovered, color);
}

void Button::SetNormalBackground(const Ref<Texture2D>& texture)
{
    SetBackgroundTexture(EUIVisualLayer::Normal, texture);
}
```

Eles não devem ganhar novas variações. Após os call sites usarem a API genérica,
remover os wrappers e os campos legados em um diff separado.

### 7.3 Componentes futuros

`TextField` e `Checkbox` podem adotar `UIStyleSet`, mas não devem ser
forçados para o mesmo diff de `Button`. Cada componente decide quais campos
consome; por exemplo, um checkbox pode ignorar `ForegroundColor`, enquanto um
text field pode usar `Focused` futuramente para um focus ring.

## 8. Matriz de comportamento obrigatório

| Estados ativos | Layers aplicadas | Resultado para uma mesma propriedade |
| --- | --- | --- |
| nenhum | `Normal` | valor de `Normal` |
| `Hovered` | `Normal -> Hovered` | `Hovered`, se declarado; senão `Normal` |
| `Pressed` | `Normal -> Pressed` | `Pressed`, se declarado; senão `Normal` |
| `Hovered + Pressed` | `Normal -> Hovered -> Pressed` | `Pressed`, se declarado; senão `Hovered`, depois `Normal` |
| `Disabled` | `Normal -> Disabled` | `Disabled`, se declarado; senão `Normal` |
| `Hovered + Disabled` | `Normal -> Hovered -> Disabled` | `Disabled`, se declarado; senão `Hovered`, depois `Normal` |
| `Pressed + Disabled` | `Normal -> Pressed -> Disabled` | `Disabled`, se declarado; senão `Pressed`, depois `Normal` |
| `Hovered + Pressed + Disabled` | `Normal -> Hovered -> Pressed -> Disabled` | `Disabled`, se declarado; senão `Pressed`, depois `Hovered`, depois `Normal` |

## 9. Testes necessários

Criar testes unitários para `UIStyleSet::Resolve` sem depender de janela,
renderização Vulkan ou input real.

1. **Normal completo** — nenhum estado ativo devolve os valores de `Normal`.
2. **Hover parcial** — `Hovered` altera somente cor; textura, borda e outline
   continuam em `Normal`.
3. **Pressed vence hover** — uma propriedade declarada em ambos devolve o valor
   de `Pressed` com os dois bits ativos.
4. **Disabled vence pressed e hover** — uma propriedade declarada em todas as
   camadas devolve o valor de `Disabled`.
5. **Fallback de disabled** — se `Disabled` não declara uma propriedade,
   preserva o valor resolvido em `Pressed`, `Hovered` ou `Normal`.
6. **Limpeza explícita de textura** — `Normal` tem textura e `Pressed` contém
   `BackgroundTexture = Ref<Texture2D>{}`; o resultado não tem textura.
7. **Layer inativa não interfere** — valor configurado em `Pressed` não aparece
   para `Hovered` sem o bit `Pressed`.
8. **API do componente** — alterar qualquer estilo e alternar hover/press/
   enabled marca o componente para novo render; desabilitar bloqueia interação.
9. **Regressão visual** — um `Button` configurado somente com as APIs antigas
   temporárias produz os mesmos draw commands de antes da migração.

## 10. Fora de escopo desta etapa

* temas globais, herança entre estilos de widgets e seletores CSS-like;
* animação/interpolação entre estilos;
* serialização de estilos em assets/editor;
* regras arbitrárias de combinação, como `Selected + Focused + Hovered`;
* modificar layout em resposta a uma layer visual;
* tornar `Disabled` sinônimo de `Hidden`, `Collapsed` ou qualquer valor de
  `EVisibility`.

Essas extensões podem reutilizar `SUIStyleOverride` e o resolvedor, mas devem
ser propostas com sua própria semântica e testes de precedência.

## 11. Critérios de aceite

* Não há nova API pública específica para um único estado (`SetPressedColor`,
  `SetDisabledBackground`, etc.).
* A precedência observável é sempre `Disabled > Pressed > Hovered > Normal`.
* Overrides parciais herdam corretamente propriedades das layers anteriores.
* É possível limpar explicitamente uma textura herdada.
* O renderer consome apenas `SResolvedUIStyle`; não replica condicionais de
  precedência em cada propriedade.
* Desabilitar altera aparência e bloqueia interação, sem alterar layout ou
  visibilidade.
* Os defaults atuais de `Button` permanecem visualmente equivalentes após a
  migração.
* Comentários internos existem apenas quando explicam uma decisão, restrição ou
  risco que não é evidente no código; todos os métodos públicos novos ou
  alterados têm documentação breve de contrato, em linguagem simples conforme
  a ISO 24495-1:2023.
