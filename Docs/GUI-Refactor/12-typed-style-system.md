# Sistema de estilos tipados para GUI

## Objetivo

Definir a aparência padrão da GUI em um único lugar e permitir que cada
componente tenha uma aparência própria quando necessário.

O sistema não apresenta temas nomeados ao usuário. A engine fornece estilos
padrão internos. Uma aplicação pode substituí-los para alterar a aparência
global. Um componente pode receber um estilo explícito e, então, deixa de usar
os estilos globais até que o estilo seja resetado.

## Tipos principais

`SBrush` descreve uma superfície retangular. Ele contém cor, textura
nine-patch, bordas, raio de canto, outline, sombra interna e sombra externa.
`RenderBatch::AddBrush` escolhe o comando de desenho adequado: textura quando
`Texture` existe; retângulo sólido quando não existe.

`SAppearance` contém os dados genéricos que um widget pode desenhar:

```cpp
struct SAppearance
{
    SBrush Background;
};
```

Um componente estende essa aparência apenas com dados que ele usa. Por
exemplo, `SButtonAppearance` e `STextFieldAppearance` acrescentam
`Foreground`. O widget base não passa a ter uma propriedade de foreground por
causa disso.

`TStateStyles<TAppearance>` armazena uma aparência completa para `Normal`,
`Hovered`, `Pressed`, `Focused` e `Disabled`. A resolução não mistura campos
de estados diferentes. Ela retorna uma aparência completa com a seguinte
prioridade:

```text
Disabled > Pressed > Hovered > Focused > Normal
```

`Focused` é usado quando não há estado de prioridade maior. Essa regra preserva
o requisito de que `Disabled` sempre vence `Pressed`, `Hovered` e `Normal`.

## Estilos por componente

Cada componente declara seu próprio tipo completo de estilo:

```cpp
struct SButtonStyle : IWidgetStyle, TStateStyles<SButtonAppearance> {};
struct STextFieldStyle : IWidgetStyle, TStateStyles<STextFieldAppearance> {};
```

`Checkbox` usa `SCheckboxStyle`. Além dos estados desmarcados herdados de
`TStateStyles<SCheckboxAppearance>`, ele contém `Checked`, `CheckedHovered`,
`CheckedPressed`, `CheckedFocused` e `CheckedDisabled`.

`Checked` pertence ao estilo do checkbox. Não é um estado genérico de
`Widget`. O checkbox decide como mapear seu valor booleano e seus estados de
interação para uma aparência. Um componente futuro pode ter outra semântica
sem ampliar o modelo genérico.

## Estilos globais e overrides locais

`GetDefaultStyles()` retorna o registro global de estilos padrão. O registro
é um `StyleSet` tipado. Ele é consultado quando cada widget é construído:

```cpp
SButtonStyle button = GetDefaultStyles().GetWidgetStyle<SButtonStyle>();
button.Hovered.Background.Color = hoverColor;
GetDefaultStyles().SetWidgetStyle(button);
```

`GetWidgetStyle` retorna uma referência constante. Para alterar o padrão,
copie o estilo, altere a cópia e use `SetWidgetStyle`. A alteração afeta os
widgets criados depois dela. Widgets existentes mantêm o estilo que receberam
na construção.

Um componente pode substituir seu estilo completo por `SetStyle`:

```cpp
SCheckboxStyle checkboxStyle = GetDefaultStyles().GetWidgetStyle<SCheckboxStyle>();
checkboxStyle.Checked.Background.Color = accentColor;
checkbox->SetStyle(checkboxStyle);
```

O componente sempre é dono desse valor. Não existe `ResetStyle`: voltar a um
valor anterior é uma decisão do chamador, que pode guardar e reaplicar o estilo
que desejar.

## Compatibilidade de transição

Os setters por `EStyleLayer`, como `SetBackgroundColor`, continuam disponíveis
durante a migração do editor. Eles alteram diretamente o estilo que o widget
recebeu na construção.

Código novo deve montar um estilo completo e chamar `SetStyle`. Isso torna a
origem da aparência explícita e evita uma coleção crescente de setters por
propriedade e por estado.

## Documentação e comentários

A documentação pública usa frases diretas, termos consistentes e descreve o
efeito observável de cada API. Ela segue os princípios de linguagem simples da
ISO 24495-1:2023. Comentários de implementação aparecem apenas quando explicam
uma decisão, uma limitação ou um risco que o código não mostra por si só.
