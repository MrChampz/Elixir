# Arquitetura de animação

Implementação em `feature/skinned-animations`, sobre `22c13eee`.

## Recursos e execução

| Conceito | Responsabilidade |
| --- | --- |
| `SLocalPose` | Transforms locais em ordem de skeleton, com validação própria. |
| `SModelPose` | Matrizes acumuladas no espaço do modelo, sem posicionamento no mundo. |
| `AnimationNode` | Contrato comum de um nó que participa da avaliação de poses. |
| `Sequence` | Tracks, markers, notifies e amostragem de uma sequência. |
| `BlendSpace` | Mistura de sequências conforme entradas numéricas. |
| `StateMachine` | Seleção de fontes e transições a partir de parâmetros. |
| `BlendNode` | Nó que compõe uma fonte base e contribuições por máscara. |
| `SequenceNode`, `BlendSpaceNode`, `StateMachineNode` | Referências aos recursos usados pelo grafo. |
| `AnimationGraph` | Parâmetros e conexão explícita com o nó de saída. |
| `Animation` | Artefato de execução com parâmetros, controles, clocks e frame publicado. |
| `SkeletalMeshInstance` | Mesh fixa e matrizes de referência para renderização sem animação. |

SequenceNode, BlendSpaceNode, StateMachineNode e BlendNode implementam `AnimationNode`.
Sequence, BlendSpace e StateMachine são recursos separados, referenciados
pelos respectivos nós. A saída pode receber um nó simples; uma máquina de estados não é
obrigatória. Nós de mistura podem alimentar outros nós ou estados dentro do mesmo grafo.

`AnimationGraph` não implementa AnimationNode. Não há LinkedGraphNode, subgrafos ou qualquer
conexão de um AnimationGraph dentro de outro. Essa restrição é verificada nos testes de tipos.

`SLocalPose` e `SModelPose` são definidos em `Pose.h` e implementados em `Pose.cpp`.
`SLocalPose` possui os transforms locais em `LocalTransforms`, na ordem dos ossos do
skeleton. Referências, amostras, transições e camadas usam esse mesmo tipo. Entradas recebem
`const SLocalPose&`; saídas recebem `SLocalPose&`. `IsValid(boneCount)` verifica o
número de ossos e a validade de cada transform. `Skeleton::BuildModelPose` acumula a hierarquia
e produz um `SModelPose`, cujo campo `Matrices` mantém uma matriz por osso. O renderer e os caminhos de
skinning recebem `const SModelPose&`; posicionamento no mundo continua no argumento `transform`.

Os assets são imutáveis e compartilhados. Cada conexão cria sua própria execução, mesmo
quando duas conexões referenciam o mesmo asset. `AnimationGraph` substitui o antigo
`AnimationController`; `Animation` não retém o grafo que a construiu.

## Composição explícita

```cpp
auto composition = BlendNode::Create("Character", {
    {
        .Name = "Locomotion",
        .Node = BlendSpaceNode::Create(locomotion),
        .InputParameters = { "Strafe", "Forward" }
    },
    {
        .Name = "UpperBody",
        .Mask = upperBodyMask,
        .Node = StateMachineNode::Create(actionStateMachine)
    }
});

auto graph = AnimationGraph::Create({
    .Name = "Character",
    .Parameters = {{ "Strafe", 0.0f }, { "Forward", 0.0f }},
    .Output = composition
});
```

O primeiro input fornece a pose base integral. Os demais aplicam máscara, peso e modo
Override ou Additive em ordem. Uma entrada pode ser outro nó `BlendNode`: seu resultado
é composto antes de contribuir para o nó pai. Não se achatam os inputs dos nós conectados. Todos pertencem à mesma definição de grafo.

O root motion vem da conexão base, preservando a política anterior das camadas. Contribuições
mascaradas alteram a pose, sem somar deslocamento de gameplay ao da base.

Os nomes dos inputs dão acesso aos controles de reprodução e montage:

```cpp
auto* action = animation->GetLayer("UpperBody");
if (action) action->PlayMontage(montage);
```

Para nós reutilizados em vários ramos, use caminhos como `Left/Action` e `Right/Action`.
Uma consulta simples ambígua retorna null. Os eventos retêm o caminho do controle de origem.
As conexões do asset permanecem fixas; pesos, reprodução e montages pertencem à execução.
Camadas adicionadas manualmente à `Animation` continuam podendo ser removidas.

## Mesh e animação independentes

```cpp
auto meshInstance = SkeletalMeshInstance::Create(mesh);
auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
if (!meshInstance || !animation) return;

animation->GetParameters().SetFloat("Forward", speed);
if (!animation->Update(deltaSeconds)) return;

renderer.Render(*meshInstance, animation->GetModelPose(), transform);
```

`SkeletalMeshInstance` não possui Animation, Play, Update ou SetAnimation. O renderer recebe
as matrizes explicitamente e não avança animações. O chamador fornece uma pose correspondente
ao skeleton da mesh. O overload `Render(instance, transform)` usa as matrizes de referência.

Dissolve mantém mesh e animação separadas e faz essa associação na submissão ao renderer.
Não foram adicionados componentes, entidades ou sistemas ECS. No futuro, esses elementos
podem assumir a associação e a atualização sem mudar os contratos dos recursos.

Uma animação também pode ser construída manualmente por `Animation::Create(skeleton, pose)`.
Um futuro sistema de Playables pode configurar fontes e composição nessa mesma fronteira.

## Avaliação e publicação

`AnimationPlayback` controla fase, looping, conclusão e seek. O `AnimationNodeEvaluator`
possui somente os contratos `Clone`, `Configure`, `GetState`, `Evaluate`, `Apply` e `Reset`.
`Configure` recebe valores já resolvidos; a interface não controla bindings numéricos nem relógio.
Máquinas conservam os índices das suas condições e configuram os playbacks dos estados.
`Evaluate` recebe `SAnimationEvaluationContext` e produz uma pose candidata, posição e descrição
emprestada das fontes em `SAnimationEvaluation`. `Apply` publica o estado dos filhos após a
validação; `Reset` descarta candidatos e reposiciona os filhos quando o playback faz seek.

`AnimationInputBindings` retém os valores, parâmetros e índices de inputs, validando alterações
antes de substituir o binding anterior. A atualização normal reutiliza o buffer de valores.
`AnimationNodeQuery` consulta camadas, caminhos e estados de execução descritos pelos nós;
`GetLayer` e `GetCurrentState` não fazem parte da interface do avaliador.

Os consumidores leem as fontes efetivamente escolhidas pela avaliação: contribuições de
sequências, um playback selecionado com sua transição, ou camadas com seus frames candidatos.
Essas descrições são vistas emprestadas e não devem sobreviver a uma alteração do avaliador.
`AnimationNotifyTracker` transforma os intervalos em eventos e agrega os eventos dos filhos.
`RootMotion` extrai e mistura movimento a partir das mesmas fontes e relógios. Os avaliadores
não expõem métodos de intervalos de playback, notifies ou root motion.

Cada tipo de nó cria seu avaliador por `CreateEvaluator()`: `SequenceEvaluator`,
`BlendSpaceEvaluator`, `StateMachineEvaluator` ou `BlendNodeEvaluator`. Esses avaliadores
mantêm os dados de execução específicos de cada conexão, separados do recurso compartilhado.
`BlendNodeEvaluator` avalia os filhos e compõe suas poses antes de publicar seus estados.
Transições de máquinas aninhadas, clocks e notifies só são aplicados após a composição final
e a construção da pose em espaço do modelo terem sucesso.

A criação de uma Animation configura todos os parâmetros e controles e compõe a pose inicial.
Ela não depende de inicialização adiada no primeiro Update e não avalia transições durante
a criação. A pausa global congela clocks e decisões, preservando cancelamentos pendentes.

Notifies de todos os inputs ativos são agregados. Silenciamento cancela as ativações e a
retomada cria novas identidades. Uma saída de montage no meio do frame retoma as notificações
do grafo somente no intervalo restante, preservando offsets e duração dos Updates.

`AnimationTransition`, `AnimationMontagePlayback` e `AnimationNotifyTracker` possuem os estados
de seus respectivos conceitos. `SLocalPose` concentra validação e blending local; `SModelPose` valida matrizes acumuladas.
`RootMotion` contém as operações de movimento.
Os contextos de root motion validam referência, fase, duração e intervalos de montage.
A extração de uma composição consulta a camada base por sua API de movimento, preservando
transições e montages também quando o intervalo começa no meio do frame. Os intervalos
avaliados da montage são recortados ao trecho solicitado, e o tempo após sua saída continua
pela fonte da camada, com o offset original.

O blending de root motion ainda usa o peso médio do fade sobre cada deslocamento extraído.
Essa aproximação não garante o mesmo resultado ao dividir um frame quando a velocidade do
root varia ou quando translação e rotação se combinam. Uma regressão documenta o caso de um
root que só se move na segunda metade de um fade: uma atualização de um segundo resulta em
deslocamento 0,5 e rotação 45°, enquanto duas de meio segundo resultam em 0,75 e 67,5°.
Eliminar essa diferença exige integrar o movimento ponderado ao longo do fade.

Cada header tem um único `.cpp` para suas implementações fora do header; templates e acessores
simples permanecem no header.

As regras estruturais pertencem às próprias definições. Fábricas validam assets; o binding
valida tipos e resolve os nomes dos parâmetros de inputs e condições para índices. Assets
imutáveis não são revalidados integralmente por frame. O BlendSpace reutiliza as contribuições
resolvidas enquanto suas entradas não mudam.

## Validação e limites

Os testes cobrem fontes simples, grafos aninhados, reutilização de nós, independência entre
personagens, máscaras, additive, montages, notifies, root motion e rollback de estados.
Os testes Vulkan exercitam o skinning com poses fornecidas por Animation independente,
incluindo um lote de mil personagens.

`AnimationTestCharacter` e `AnimationGraphTestDriver` existem somente em Tests para preservar
os fixtures de regressão. Não são componentes nem APIs do Engine. Os novos testes de composição
usam diretamente AnimationGraph, Animation e SkeletalMeshInstance separados.

Novas APIs fora dos headers usam `ELIXIR_API`. A execução local valida macOS arm64; compilação
e linking reais no Windows dependem da CI. Não foi medido um benchmark comparável de CPU e
alocações antes/depois. O caminho de avaliação reutiliza buffers, mas não promete zero alocações.

Validação desta revisão: build de Dissolve e Elixir.UnitTests aprovado, 694 testes de CPU e
26 testes Vulkan aprovados. No Dissolve foram conferidos ataque, janela de hitbox, recovery
e retorno à animação contínua, com mesh e pose submetidas separadamente. O log da execução
não registrou erros ou falhas de validação Vulkan.
