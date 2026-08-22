# Adequação do sistema de estilos para temas e variantes de Checkbox

## 1. Objetivo

Evoluir o sistema de estilos atual para que ele suporte, sem duplicar estilos
em cada widget:

* temas compartilhados por classe de componente;
* overrides locais esparsos;
* composição determinística de estados visuais;
* `Checked` como uma variante que o tema pode estilizar;
* futura variante `Indeterminate`, sem uma nova arquitetura.

Esta é uma especificação de migração. Ela descreve o estado atual, o destino e
as fases de alteração. Não autoriza uma alteração de comportamento fora dos
itens e critérios de aceite definidos aqui.

## 2. Estado atual confirmado

O código atual já possui uma primeira versão do sistema de estilos:

| Área | Estado atual |
| --- | --- |
| `Style.h` | `EStyleLayer`, `EInteractionState`, `SStyleOverride`, `SResolvedStyle` e `StyleSet` existem. |
| `StyleSet` | Armazena um `std::array<SStyleOverride, Count>` em cada instância e resolve `Normal -> Hovered -> Pressed -> Focused -> Disabled`. |
| `Widget` | Possui `StyleSet m_Styles`, expõe setters por `EStyleLayer` e monta `Hovered`, `Pressed`, `Focused` e `Disabled` em `GetInteractionState()`. |
| `Button` | Cria todos os defaults de aparência por instância, no construtor. |
| `TextField` | Também cria seus defaults por instância e usa `Focused` para alterar o estilo. |
| `Checkbox` | Usa o estilo resolvido quando desmarcado, mas mantém `m_CheckedColor` fora de `StyleSet` e escolhe cor/outline com um `if (m_Checked)` próprio. |

O sistema atual resolve corretamente os estados de interação já conhecidos,
mas não pode representar um estilo específico para `Checked + Hovered`, nem
compartilhar defaults entre instâncias por meio de um tema.

## 3. Decisões obrigatórias

### 3.1 O valor do checkbox continua no widget

`Checkbox` continua sendo dono do seu estado semântico:

```cpp
ECheckState m_CheckState = ECheckState::Unchecked;
```

O tema e as regras de estilo nunca alteram esse valor. Eles apenas recebem uma
fotografia dele durante a resolução. Assim, `SetChecked`, callbacks e input
continuam pertencendo ao `Checkbox`; a camada de estilo permanece declarativa.

### 3.2 `Checked` entra no seletor de estilo

`Checked` passa a ser um estado visual selecionável. Isso permite que o tema
defina propriedades para caixas marcadas, desmarcadas, hovered, pressionadas e
desabilitadas sem que `Checkbox::BuildDrawCommands` escolha cores ou outlines
por conta própria.

O estado desmarcado é o default: regras sem o bit `Checked` formam a aparência
de `Unchecked`. Regras com o bit `Checked` refinam essa aparência quando a
caixa está marcada.

### 3.3 `Indeterminate` deve ser previsto agora

O booleano público atual pode continuar na primeira fase por compatibilidade,
mas a representação interna deve migrar para:

```cpp
enum class ECheckState : uint8_t
{
    Unchecked,
    Checked,
    Indeterminate,
};
```

Inicialmente, `SetChecked(true)` mapeia para `Checked` e
`SetChecked(false)` para `Unchecked`. `GetChecked()` retorna `true` somente
para `Checked`. Uma API específica para `Indeterminate` fica fora da primeira
fase, mas a estrutura de seleção já deve conseguir representá-lo.

`Checked` e `Indeterminate` são variantes mutuamente exclusivas. Eles não
devem ser bits combináveis da mesma máscara.

### 3.4 Prioridade de interação

Para uma mesma propriedade, a prioridade mínima obrigatória é:

```text
Disabled > Focused > Pressed > Hovered > Normal
```

Isso preserva a ordem que o resolvedor atual usa para `Focused` e mantém as
garantias já acordadas de que `Disabled` vence `Pressed`, `Hovered` e `Normal`.

`Checked` e `Indeterminate` não competem com essa prioridade. Eles são
variantes persistentes aplicadas antes dos estados transitórios. Portanto, uma
cor de `Checked + Hovered` pode sobrescrever uma cor genérica de `Hovered`, mas
uma regra de `Disabled` ainda vence ambas.

## 4. Modelo de dados de destino

### 4.1 Contexto de resolução

Substituir `EInteractionState` como entrada pública do resolvedor por um
contexto que separa interação de variante semântica:

```cpp
enum class EInteractionState : uint8_t
{
    None     = 0,
    Hovered  = 1 << 0,
    Pressed  = 1 << 1,
    Focused  = 1 << 2,
    Disabled = 1 << 3,
};

enum class EStyleVariant : uint8_t
{
    Default,
    Checked,
    Indeterminate,
};

struct SStyleContext
{
    EInteractionState Interaction = EInteractionState::None;
    EStyleVariant Variant = EStyleVariant::Default;
};
```

`EStyleVariant` descreve uma alternativa semântica exclusiva, e não uma ação
do ponteiro ou teclado. `Button` e `TextField` usam `Default`; `Checkbox`
converte seu `ECheckState` para a variante correspondente.

Uma futura classe de componente pode adicionar variantes somente após definir
sua semântica. Não usar `Checked` para expressar `Selected`, `Active` ou
estados de domínio de outros componentes.

### 4.2 Seletor e regra esparsa

```cpp
struct SStyleSelector
{
    EInteractionState Required = EInteractionState::None;
    EInteractionState Forbidden = EInteractionState::None;
    std::optional<EStyleVariant> Variant;
};

struct SStyleRule
{
    SStyleSelector Selector;
    SStyleOverride Override;
};
```

Uma regra corresponde quando:

```cpp
bool Matches(const SStyleSelector& selector, const SStyleContext& context)
{
    return HasAll(context.Interaction, selector.Required)
        && HasNone(context.Interaction, selector.Forbidden)
        && (!selector.Variant || *selector.Variant == context.Variant);
}
```

`Forbidden` é necessário para expressar um caso genuinamente exclusivo, como
uma regra que se aplica somente quando o widget não está focused. Não deve ser
usado para reproduzir a prioridade normal entre hover, press e disabled; a
ordem do resolvedor já cobre essa necessidade.

### 4.3 Armazenamento esparso

Substituir o array fixo atual de `StyleSet` por regras declaradas apenas quando
necessárias:

```cpp
class ELIXIR_API StyleSet
{
  public:
    const SStyleRule* Find(SStyleSelector selector) const;
    void Set(SStyleSelector selector, const SStyleOverride& style);
    void Clear(SStyleSelector selector);

  private:
    std::vector<SStyleRule> m_Rules;
};
```

`std::vector` vazio não aloca os `SStyleOverride` que não são usados. Com o
número esperado de regras por componente, busca e remoção lineares são
aceitáveis e mais simples que uma tabela grande ou hash map. `Set` deve manter
no máximo uma regra para o mesmo seletor.

Não usar `std::optional<SStyleOverride>` em um array como solução de memória:
o `optional` ainda reserva espaço inline para o `SStyleOverride`.

## 5. Tema e fontes de estilo

### 5.1 Classes estilizadas

```cpp
enum class EStyleClass : uint8_t
{
    Button,
    TextField,
    Checkbox,
};

class ELIXIR_API Theme
{
  public:
    const StyleSet* FindStyle(EStyleClass styleClass) const;
    void SetStyle(EStyleClass styleClass, StyleSet style);

  private:
    std::unordered_map<EStyleClass, StyleSet> m_Styles;
};
```

`Theme` é dono dos defaults compartilhados de uma classe. Um tema pode não
declarar uma classe; nesse caso, o componente usa seus defaults de segurança
ou o tema base configurado pelo `Manager`.

Cada `Widget` precisa de:

```cpp
EStyleClass m_StyleClass;
Ref<const Theme> m_Theme;
StyleSet m_LocalStyleOverrides;
```

O widget não copia regras do tema para `m_LocalStyleOverrides`. Ele mantém a
referência ao tema e cria uma regra local apenas quando o usuário configura
aquele widget.

### 5.2 Ordem correta entre tema e override local

Não resolver primeiro todo o tema e depois todos os overrides locais. Isso
permitiria que um override local de `Normal` apagasse o `Disabled` definido no
tema.

A composição obrigatória é por nível de prioridade:

```text
defaults[Normal]
tema[Normal]                 -> local[Normal]
tema[Checked]                -> local[Checked]
tema[Hovered]                -> local[Hovered]
tema[Checked + Hovered]      -> local[Checked + Hovered]
tema[Pressed]                -> local[Pressed]
tema[Checked + Pressed]      -> local[Checked + Pressed]
tema[Focused]                -> local[Focused]
tema[Disabled]               -> local[Disabled]
tema[Checked + Disabled]     -> local[Checked + Disabled]
```

Uma regra só participa se seu seletor corresponder ao contexto atual. Para
regras de mesma prioridade, o tema é aplicado primeiro e a regra local depois.
Assim, customização local é possível sem violar a prioridade de estados.

### 5.3 Defaults de segurança

`SResolvedStyle` não pode depender de um tema para ser inicializado. Cada
componente deve ter um `SResolvedStyle` completo de segurança, usado como
primeira fonte da composição:

```text
fallback completo -> tema correspondente -> overrides locais
```

Os valores hoje criados no construtor de `Button`, `TextField` e `Checkbox`
devem se tornar esses defaults temporários ou ser movidos para um tema base.
Enquanto um tema base não existir, o fallback preserva a aparência atual e
evita campos default-constructed sem significado no renderer.

## 6. Algoritmo de resolução

O resolvedor recebe três entradas: fallback completo do componente, regras do
tema e regras locais. Ele aplica somente regras que correspondem ao contexto,
ordenadas por um ranking interno fixo; não aceita uma prioridade numérica
configurável pelo usuário.

```cpp
SResolvedStyle ResolveStyle(
    const SResolvedStyle& fallback,
    const StyleSet* themeStyles,
    const StyleSet& localStyles,
    const SStyleContext& context)
{
    SResolvedStyle result = fallback;

    for (const SStyleSelector& selector : ResolutionOrder(context))
    {
        if (themeStyles)
            ApplyMatchingRules(result, *themeStyles, selector, context);

        ApplyMatchingRules(result, localStyles, selector, context);
    }

    return result;
}
```

`ResolutionOrder` é um detalhe interno e deve produzir uma sequência estável.
Para checkbox marcado hovered, por exemplo, a sequência relevante é:

```text
Default -> Checked -> Hovered -> Checked + Hovered
```

Para checkbox marcado, pressionado e disabled:

```text
Default -> Checked -> Pressed -> Checked + Pressed -> Disabled -> Checked + Disabled
```

Se houver duas regras na mesma fonte que corresponderiam com a mesma
especificidade e prioridade, `StyleSet::Set` deve rejeitar a duplicata ou
substituí-la. A resolução não deve depender de ordem de inserção acidental.

## 7. Alterações por componente

### 7.1 `Widget`

1. Trocar `m_Styles` por `m_LocalStyleOverrides`.
2. Armazenar `m_StyleClass`, `m_Theme` e o fallback completo de estilo.
3. Substituir `GetInteractionState()` por `GetStyleContext()` virtual.
4. A implementação base preenche `Hovered`, `Pressed`, `Focused` e `Disabled`
   em `SStyleContext::Interaction`, com `Variant = Default`.
5. `GetResolvedStyle()` chama o resolvedor com fallback, tema e overrides
   locais.
6. Manter `SetStyle`, `ClearStyle` e setters de propriedades, mas mudar seus
   parâmetros de `EStyleLayer` para `SStyleSelector` ou helpers tipados de
   selector.

Exemplo de helper de chamada frequente:

```cpp
constexpr SStyleSelector HoveredStyle()
{
    return { .Required = EInteractionState::Hovered };
}

constexpr SStyleSelector CheckedHoveredStyle()
{
    return {
        .Required = EInteractionState::Hovered,
        .Variant = EStyleVariant::Checked,
    };
}
```

Helpers devem existir somente para seletores usados com frequência. Não criar
um helper para cada combinação possível.

### 7.2 `Button`

1. Definir `m_StyleClass = EStyleClass::Button`.
2. Mover os `SStyleOverride` hoje criados no construtor para o tema base
   `Button`, preservando os mesmos valores.
3. Converter a outline de foco atual para uma regra com
   `Required = Focused`.
4. Remover configuração local de defaults após o tema base estar disponível.
5. Preservar `SetTextColor` somente como nome específico que encaminha para o
   setter de foreground baseado em seletor.

### 7.3 `TextField`

1. Definir `m_StyleClass = EStyleClass::TextField`.
2. Migrar background normal, outline focused e aparência disabled para o tema
   base da classe.
3. Manter cursor, seleção e placeholder fora desta etapa: eles são elementos
   internos do campo, não o estilo de superfície compartilhado.

### 7.4 `Checkbox`

1. Definir `m_StyleClass = EStyleClass::Checkbox`.
2. Substituir `bool m_Checked` por `ECheckState m_CheckState`.
3. Implementar `GetStyleContext()` sobrescrito. Ele chama a versão de `Widget`
   e define `Variant` como `Checked` ou `Indeterminate` conforme
   `m_CheckState`.
4. Remover `m_CheckedColor`, `GetCheckedColor` e `SetCheckedColor`.
5. Remover de `BuildDrawCommands` a seleção manual de `color` e `outline` com
   `if (m_Checked)`.
6. Desenhar exclusivamente o `SResolvedStyle` retornado pelo resolvedor.
7. Mover a aparência atual para o tema base:

```text
Checkbox/Default:           fundo escuro, raio 3, outline cinza.
Checkbox/Hovered:           fundo um pouco mais claro.
Checkbox/Checked:           preenchimento azul, sem outline.
Checkbox/Disabled:          fundo com alpha reduzido.
Checkbox/Checked+Disabled:  azul desabilitado, sem outline.
```

O tema deve declarar `Checked + Disabled` explicitamente. Caso contrário, a
regra `Disabled` pode redefinir o fundo azul de `Checked` com o fundo cinza
desabilitado, que não é o comportamento visual desejado.

O desenho inicial de `Indeterminate` pode reutilizar a aparência `Checked`.
Um traço horizontal ou ícone próprio exige suporte de renderer e fica fora
desta migração.

## 8. Migração compatível e ordem de entrega

### Fase 1 — Infraestrutura sem alteração visual

* Adicionar `SStyleContext`, `EStyleVariant`, `SStyleSelector` e `SStyleRule`.
* Fazer `StyleSet` armazenar regras esparsas e resolver somente uma fonte.
* Cobrir o resolvedor com testes de seletor, fallback e precedência.
* Manter uma ponte temporária de `EStyleLayer` para seletor simples, se isso
  reduzir o tamanho do diff de call sites.

### Fase 2 — Tema base e composição de fontes

* Adicionar `Theme`, `EStyleClass` e uma referência de tema no `Widget`.
* Implementar composição por selector: tema antes de local para cada nível.
* Migrar os defaults atuais de `Button` e `TextField` para o tema base.
* Validar que widgets sem override local preservam os draw commands atuais.

### Fase 3 — Checkbox como variante de estilo

* Introduzir `ECheckState` internamente e preservar `GetChecked`/`SetChecked`.
* Migrar a aparência checked para regras `EStyleVariant::Checked` do tema.
* Remover `m_CheckedColor` e o `if (m_Checked)` de desenho.
* Cobrir `Default`, `Checked`, `Hovered`, `Checked + Hovered`, `Disabled` e
  `Checked + Disabled`.

### Fase 4 — Limpeza de API

* Remover a ponte de `EStyleLayer`, se usada.
* Remover construtores que copiam defaults de tema por instância.
* Documentar a API pública final e atualizar exemplos/call sites.

Cada fase deve compilar, ter testes próprios e manter a aparência existente,
exceto onde a mudança visual estiver declarada e aprovada.

## 9. Testes obrigatórios

### 9.1 `StyleSet`

* uma regra `Hovered` corresponde com e sem variante `Checked`;
* uma regra com variante `Checked` não corresponde ao checkbox desmarcado;
* uma regra com variante `Indeterminate` não corresponde a `Checked`;
* `Forbidden = Focused` exclui corretamente contexto focused;
* regras duplicadas para o mesmo seletor têm comportamento explícito;
* `SStyleOverride` parcial preserva os campos resolvidos anteriormente;
* `BackgroundTexture = Ref<Texture2D>{}` limpa uma textura herdada.

### 9.2 Prioridade e fontes

* `Disabled` vence `Focused`, `Pressed`, `Hovered` e `Normal`;
* `Pressed` vence `Hovered`;
* `Checked + Hovered` vence a regra genérica de `Hovered` somente para a
  variante checked;
* `Checked + Disabled` vence `Disabled` e preserva a semântica visual checked;
* override local de `Normal` não sobrescreve `Disabled` do tema;
* override local de `Disabled` sobrescreve `Disabled` do tema.

### 9.3 Regressões de componente

* `Button` e `TextField` sem overrides locais geram os mesmos draw commands
  que antes da migração;
* `Checkbox` desmarcado mantém fundo, raio e outline existentes;
* `Checkbox` marcado usa estilo do tema, sem caminho especial em
  `BuildDrawCommands`;
* `SetChecked` não dispara `OnCheckedChanged`;
* um clique em checkbox desabilitado não muda `ECheckState`;
* desabilitar entre mouse-down e mouse-up não ativa checkbox nem mantém visual
  pressed.

## 10. Regras de documentação e comentários

Código e documentação pública devem seguir linguagem simples, conforme os
princípios da ISO 24495-1:2023:

* usar frases diretas, curtas e termos consistentes;
* documentar todo método público novo ou alterado com seu contrato essencial;
* incluir `@param` e `@return` apenas quando ajudam o uso correto;
* usar comentários internos somente para uma decisão, risco ou restrição que
  não seja evidente por nomes claros e código pequeno;
* não comentar atribuições, chamadas diretas ou condições autoexplicativas;
* manter a regra de precedência neste documento e nos testes, em vez de repetir
  explicações extensas em cada método.

Ao migrar os arquivos atuais, reduzir comentários internos redundantes. Devem
permanecer, por exemplo, comentários que expliquem a composição tema/local por
selector e a guarda de ativação pendente após `SetEnabled(false)`.

## 11. Fora de escopo

* carregar, serializar ou editar temas no Editor;
* animação entre regras de estilo;
* herança de tema por árvore de widgets;
* hot reload de arquivos de tema;
* modificadores arbitrários de layout por estado visual;
* ícone de check ou traço de indeterminado;
* variantes semânticas para componentes além de Checkbox.

## 12. Critérios de aceite

* `Checkbox` não possui cor ou outline checked fora do resolvedor de estilo.
* Um tema define estilos de `Button`, `TextField` e `Checkbox` uma vez e eles
  são compartilhados pelas instâncias.
* Overrides locais são esparsos e não reservam um `SStyleOverride` completo por
  layer em cada `Widget`.
* A composição é feita por nível de prioridade, com tema antes de local no
  mesmo nível.
* `Disabled > Focused > Pressed > Hovered > Normal` é determinístico para toda
  propriedade que mais de uma regra declara.
* `Checked` e `Indeterminate` são variantes exclusivas e podem receber regras
  diferentes no tema.
* Cada fase tem testes unitários e regressões de draw command proporcionais ao
componente migrado.
* Documentação pública e comentários seguem as regras da seção 10.
