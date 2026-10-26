# Plano de refatoração da animação

Proposta baseada na branch `feature/skinned-animations`, revisão `22c13eee`.
Este documento registra a proposta original e sua sequência de migração.
O refactor foi aplicado no working tree em 24/10/2026. A arquitetura evoluiu para
`AnimationGraph` com saída explícita, nós `AnimationNode` e `BlendNode`, sem grafos ligados, e execução em `Animation`,
separada de `SkeletalMeshInstance`. Este documento preserva o plano original; os contratos
entregues e as decisões posteriores estão em
[AnimationArchitecture.md](AnimationArchitecture.md).
Revisado em 24/10/2026 para incluir fontes polimórficas, validação autocontida e um único
arquivo de implementação por header.

## 1. Objetivo

Permitir entender cada conceito isoladamente e localizar sua implementação sem percorrer
uma classe que conhece todo o sistema. Preservar sequências, blend spaces, sincronização,
transições, camadas override e aditivas, montages, notifies e root motion.

A regra central é: assets descrevem a animação, objetos de reprodução mantêm seu progresso,
controladores escolhem o que reproduzir e camadas determinam como compor o resultado.

O alvo é composição de componentes com contratos pequenos e uma interface comum para
fontes. Sequências, blend spaces e máquinas de estados devem ser aplicáveis pela mesma API.
O polimorfismo fica nessa fronteira; não é necessário criar uma hierarquia para cada
operação matemática ou um framework de plugins.

Cada classe terá um par único `.h`/`.cpp`, contendo no `.cpp` toda a implementação fora do
header. Nunca dividir a implementação de uma classe por feature em vários `.cpp`.
Quando um conceito exigir separação, extrair sua classe ou seu módulo com header próprio.
Métodos de templates que precisem permanecer no header não criam arquivos de implementação
adicionais para a mesma classe.

## 2. Diagnóstico do código atual

| Local | Responsabilidades misturadas | Consequência |
| --- | --- | --- |
| `AnimationLayer` | Reprodução, relógios, cross-fades, montagem de pose, montages, notifies e root motion | Alterar um conceito exige conhecer os demais. Dividir os `.cpp` não separou o estado nem os contratos. |
| `AnimationLayer::SPlayback` | Sequência, blend space, coordenadas e tempo | Dois ponteiros representam alternativas; `Time` muda de unidade conforme a fonte. |
| `AnimationLayer::SLayerEvaluation` | Pose, transição, cursor de montage, notifies e próximos relógios | A representação do frame depende dos detalhes privados de uma classe muito abrangente. |
| `SkeletalMeshInstance` | Mesh fixa, camadas, atualização, composição, eventos e extração de movimento | O objeto da mesh também funciona como a instância inteira de animação. |
| `AnimationControllerInstance` | Parâmetros, inicialização de camadas, seleção de fontes e execução de máquinas de estados | A máquina de estados não pode ser compreendida ou testada independentemente do controller e da mesh. |
| `AnimationController` | Definições públicas, validação, resolução de bindings e dados compilados | As mesmas alternativas aparecem em representações diferentes; uma configuração pode selecionar fontes incompatíveis simultaneamente. |
| `BlendSpace` | Geometria de mistura e operações que resolvem contribuições repetidamente | `GetDuration`, `Sample`, `GetNotifySource` e `ExtractRootMotion` chamam `FindSamples` separadamente. |
| Avaliação temporal | Avanço da pose e reconstrução de intervalos para movimento e eventos | Há vários lugares que precisam concordar sobre início, fim, loops e saltos. |

`AnimationLayer` tem cerca de 1.500 linhas de implementação em três arquivos, além de um
header de aproximadamente 630 linhas. O tamanho é um sintoma; o problema principal é a
quantidade de conceitos que compartilham seus membros privados.

Há também uma lacuna transacional a tratar explicitamente: o controller altera fontes e
estados antes de chamar `SkeletalMeshInstance::Update`. O rollback da avaliação da mesh não
significa rollback de toda a atualização do controller.

## 3. Conceitos e responsabilidades propostas

### Assets compartilhados

Manter `Skeleton`, `Sequence`, `BlendSpace`, `BlendSpace1D`, `BlendSpace2D`,
`AnimationMontage`, `BoneMask` e `AnimationController` como dados compartilhados imutáveis.
Extrair também `StateMachine` como uma definição imutável de estados e transições.
`Sequence`, `BlendSpace` e `StateMachine` implementam `AnimationSource`.

Uma sequência continua responsável pelas suas tracks, markers, notifies e consultas locais.
Ela não passa a possuir relógio, ativações de notifies, estado do personagem ou referências
ao renderer. Seus métodos ficam em `Sequence.cpp`. Um algoritmo que ganhar um
módulo próprio deve declarar seu contrato em um header próprio e não implementar membros
de `Sequence` em outro arquivo.

Um blend space descreve a combinação de amostras. A fonte de sincronização fixa e a escolha
da amostra dominante para notifies continuam sendo políticas distintas. A refatoração não
deve trocar a primeira por uma liderança dinâmica como efeito colateral.

### Componentes em execução

| Conceito | Responsabilidade | Estado que possui |
| --- | --- | --- |
| `AnimationSource` | Interface imutável de uma fonte aplicável à animação | Contrato de skeleton, entradas e criação de uma reprodução independente |
| `AnimationSourceInstance` | Interface interna da execução de uma fonte por reprodução | Contrato comum de avaliação e publicação do estado candidato |
| `AnimationPlayback` | Oferecer reprodução uniforme de qualquer `AnimationSource` | Fonte e instância concreta criada por ela, além dos controles comuns |
| `SequencePlayback` | Avançar e amostrar uma sequência | Tempo em segundos e regras de reprodução da sequência |
| `BlendSpacePlayback` | Resolver e amostrar um blend space | Fase, entradas, contribuições e sincronização |
| `StateMachineInstance` | Executar uma máquina e produzir seu resultado de animação | Estado atual, reproduções dos estados e transição entre eles |
| `AnimationTransition` | Fazer a passagem entre reproduções | Duração, progresso e origem anterior ou pose congelada |
| `AnimationMontagePlayback` | Executar uma ação com seções sobre a fonte contínua de uma camada | Montage, cursor, links locais, envelope e interrupção |
| `AnimationNotifyTracker` | Transformar percursos selecionados em eventos e manter ativações | Identidades das ativações, notifies ativos e encerramentos pendentes |
| `AnimationLayer` | Coordenar sua fonte e ação, e definir a contribuição da camada | Máscara, peso, blend, referência aditiva e componentes de execução |
| `AnimationParameters` | Manter valores tipados e seus bindings | Layout validado e valores por personagem |
| `AnimationInstance` | Coordenar camadas e publicar um frame coerente | Contexto fixo, parâmetros, camadas e resultado final |
| `SkeletalMeshInstance` | Associar uma mesh fixa à sua animação | Mesh e uma `AnimationInstance` |

`AnimationTransition` é a transição entre reproduções. `SAnimationTransition` continua sendo
a regra declarativa de transição entre estados. A primeira não conhece condições, nomes de
estados ou parâmetros do controller.

Uma máquina de estados é uma fonte composta: escolhe um estado e avalia a fonte desse
estado. Seu resultado tem o mesmo contrato de pose, percurso, notifies e root motion que
as fontes simples. A camada não conhece seu grafo nem faz verificações para identificar
se está reproduzindo uma máquina de estados.

`AnimationController` passa a descrever parâmetros e a composição de fontes em camadas.
Cada camada referencia uma única `AnimationSource`, que pode ser uma máquina ou uma fonte
direta. Não criar máquinas artificiais com um único estado.

Com as máquinas executando dentro de suas próprias fontes e os parâmetros pertencendo à
`AnimationInstance`, a proposta final dispensa `AnimationControllerInstance`. Sua função
atual é distribuída entre configuração do controller, parâmetros e execução da fonte.
Um adaptador temporário pode preservar a API antiga durante a migração, sem relógios ou
estado duplicados.

### Operações sem estado

Separar as operações matemáticas de pose em `AnimationPose.h/.cpp`: blend, override,
additive e validação. Usar funções com entradas explícitas e spans de saída; não criar um
manager que possua personagens ou relógios.

Separar a política de root motion em funções de `RootMotion.h/.cpp`: consumir os percursos
e envelopes já calculados, combinar deltas e aplicar as regras de extração e bloqueio do
root. `Skeleton::BuildModelPose` continua sendo a conversão da pose local para matrizes.

As regras de sincronização podem ser funções internas em `AnimationSync.h/.cpp`, mantendo
os markers no asset. Um objeto `AnimationSyncManager` não é necessário neste estágio.

## 4. Fontes unificadas e playback concreto

### Interface de definição e interface de execução

`AnimationSource` é uma interface abstrata implementada pelos assets:

- `Sequence`: uma sequência de tracks e metadados.
- `BlendSpace`: uma composição de sequências, com implementações 1D e 2D existentes.
- `StateMachine`: estados, condições e transições que selecionam outras fontes.

Seu contrato é pequeno: consultar skeleton e entradas necessárias, verificar compatibilidade
com o contexto e criar uma `AnimationSourceInstance`. Não exigir duração, fase, looping ou
seek universais: uma máquina de estados não tem obrigatoriamente um único ciclo ou fim.

`AnimationSourceInstance` contém o contrato interno comum de avaliação e aplicação dos
resultados. As implementações são `SequencePlayback`, `BlendSpacePlayback` e
`StateMachineInstance`. Cada uma conhece seus dados e seus algoritmos. A definição
compartilhada nunca armazena o relógio nem o estado atual de um personagem.

A criação da implementação concreta ocorre em `AnimationSource::CreateInstance(context)`.
Esse contexto fornece skeleton, referência de pose, bindings e acesso aos parâmetros.
A avaliação recebe explicitamente o delta e buffers reutilizáveis. Não usar `dynamic_cast`,
`GetType()` seguido de `switch`, ou `if (Sequence) / if (BlendSpace) / if (StateMachine)`
nos consumidores. A extensão com outra fonte implementa o mesmo contrato.

O dispatch virtual acontece por operação da fonte, nunca por vértice, bone ou chave.
Criar instâncias e resolver bindings na aplicação da fonte; reutilizar estado e buffers nas
avaliações seguintes. Fontes compostas não recriam suas instâncias filhas a cada frame.
Validar ciclos de dependência entre assets sem proibir ciclos normais das transições de
uma máquina de estados.

A relação é:

- A camada possui `AnimationPlayback`.
- O playback retém uma `AnimationSource` imutável e possui sua instância mutável exclusiva.
- Uma fonte composta possui instâncias das suas fontes filhas.
- Cada personagem e cada reprodução têm estados independentes, mesmo compartilhando assets.

### Responsabilidade de playback

`AnimationPlayback` é o ponto de entrada concreto para aplicar e controlar uma fonte.
Ele valida a aplicação, cria a instância uma vez e delega a execução. Não incorpora todos
os algoritmos das implementações concretas nem inspeciona seus tipos a cada frame.

A API da camada deve permitir, conceitualmente:

```cpp
layer.Play(sequence);
layer.Play(blendSpace);
layer.Play(stateMachine);
```

As três chamadas usam o mesmo contrato `Play(Ref<const AnimationSource> source, ...)`.
Fade e controles comuns pertencem à reprodução/camada. Os valores e bindings requeridos
pela fonte são validados por ela; uma sequência pode não precisar de entradas, enquanto
um blend space exige coordenadas e uma máquina consulta parâmetros tipados.

Usar uma representação comum de bindings entre entradas da fonte e `AnimationParameters`.
A camada e o controller não ganham novos campos opcionais para cada tipo de fonte. Permitir
valores fixos ou parâmetros por personagem no mesmo contrato de entradas, sem um pacote
aberto de ponteiros opcionais ou payloads sem tipo.

Detalhes específicos continuam na implementação correspondente. `TimeSeconds` fica no
playback de sequência e `Phase` no playback de blend space. Nenhum campo `Time` muda de
unidade conforme o tipo. Consultas de conclusão têm semântica definida pela fonte; uma
máquina informa seu estado e sua conclusão quando houver estados terminais definidos.

Pausa comum interrompe o avanço de toda a fonte composta, preservando seus estados.
Operações específicas, como seek de uma sequência, não precisam virar métodos obrigatórios
sem significado para todas as fontes. Compatibilidade temporária de APIs específicas pode
usar controle explícito da implementação concreta, sem inspeção de tipos espalhada.

Uma camada pode ter uma pose manual ou nenhuma reprodução atribuída. Os playbacks criados
com sucesso permanecem estruturalmente válidos durante seu lifetime.

### Validação autocontida

Cada entidade conhece e valida suas próprias invariantes. Separar três perguntas:

| Pergunta | Contrato sugerido | Momento |
| --- | --- | --- |
| A definição está bem formada? | `IsValid()` ou `Validate(...)` da própria definição | Criação ou alteração de configuração |
| Ela pode ser usada neste contexto? | `IsCompatibleWith(...)` ou `CanCreateInstance(...)` | Aplicação da fonte e resolução de bindings |
| Qual é seu estado atual? | `IsPlaying()`, `IsFinished()`, `IsActive()` | Consulta durante a execução |

`IsValid()` não significa “tocando” ou “tem peso positivo”. Uma reprodução pausada, uma
transição concluída ou uma notificação pontual podem ser válidas. Usar predicados com
semântica específica para cada caso.

Exemplos de ownership das regras:

- `SAnimationNotify::IsValid(sequenceDuration)` verifica nome, tempo, duração e limite da
  sequência. O payload customizado continua opaco ao Engine.
- A definição de sequência valida tracks e delega cada notify e marker às suas regras.
- O blend space valida amostras e sua geometria; a instância valida entradas antes de
  resolver contribuições.
- A máquina valida estados, destinos e fontes filhas. No binding, valida a existência e
  os tipos dos parâmetros necessários.
- A configuração de camada valida peso, máscara, modo e referência aditiva. Compatibilidade
  de skeleton e as restrições de Base são verificadas com o contexto da instância.
- `AnimationParameters` valida nomes, tipos, índices e valores; as fontes não repetem esse
  algoritmo nos seus consumidores.

As fábricas publicam apenas assets validados. Após criação e binding bem-sucedidos, o
caminho por frame não revalida tracks, geometria e grafo completos. Setters preservam o
estado anterior quando um valor novo é inválido. Resultados produzidos em execução ainda
são verificados nas fronteiras apropriadas para detectar falhas numéricas.

Predicados de consulta não devem emitir logs. Quando for preciso explicar uma falha,
`Validate` deve fornecer o diagnóstico ao chamador, que o registra uma vez na fronteira da
operação. Usar um único mecanismo de validação por entidade; `IsValid`, se necessário, deve
reutilizá-lo em vez de manter uma segunda lista de condições.

Ainda haverá verificações de retorno e de operações permitidas. O objetivo é que cada
condição tenha um proprietário e que o consumidor leia a intenção, sem conhecer todos os
campos internos. Não centralizar regras heterogêneas em um `AnimationValidator` global.

## 5. Um percurso temporal para pose, eventos e movimento

Criar uma representação interna, `SAnimationPlaybackInterval`, para descrever os intervalos
realmente percorridos durante uma avaliação. Ela não é uma segunda notificação nem uma
nova API de gameplay.

O percurso deve identificar:

- A reprodução e o asset de origem, com lifetime garantido durante a avaliação.
- Os intervalos de fase ou tempo, com unidades explícitas e ordem cronológica.
- Os offsets em segundos dentro do frame.
- As contribuições das sequências e seus pesos, quando houver blend space.
- A inclusão do ponto inicial e as descontinuidades, como seek e salto de seção.
- O envelope temporal relevante para a mistura de movimento.

Os loops podem usar intervalos não normalizados para evitar expandir cada ciclo em um
objeto. Montages precisam registrar os trechos efetivamente visitados; um salto não cria
um intervalo cobrindo o trecho ignorado. Manter os limites de trabalho já existentes.

Há dois resultados relacionados, mas diferentes: a pose no fim do frame e o percurso
durante o frame. Root motion e notifies usam o percurso; uma pose amostrada não é suficiente
para reconstruí-los.

`AnimationNotifyTracker` recebe os percursos autorizados a emitir eventos. Cada composição
seleciona os percursos que conhece: o blend space escolhe sua amostra dominante, a transição
seleciona o destino e a camada arbitra fonte contínua versus montage. Uma máquina de estados
retorna a seleção já resolvida por suas fontes filhas. A camada não atravessa o grafo da
máquina nem inspeciona tipos. O tracker não consulta novamente coordenadas, pesos, controller
ou estado de montage.

O tracker mantém `Instant`, `Begin`, `Update` e `End` e usa somente `SAnimationNotify`, com
`Duration = 0` por padrão. Reproduzir novamente o mesmo asset deve produzir uma nova
ativação quando necessário; comparar somente o ponteiro da sequência não identifica uma
reprodução. Usar uma identidade de reprodução ou comandos explícitos de cancelamento.

Ao remover uma camada, `AnimationInstance` transfere seus encerramentos pendentes para a
fila do próximo frame válido antes de destruir o tracker. O destrutor não dispara callbacks
nem publica eventos fora do ciclo de avaliação.

Root motion usa os mesmos percursos, mas não a política de seleção de notifies. Durante
um cross-fade, o movimento pode combinar origem e destino mesmo quando os eventos da
origem estão suprimidos. O envelope para movimento considera o intervalo, não apenas o
peso da pose no final do frame.

## 6. Limites de camada, montage e instância

Uma camada possui o playback atual e uma transição opcional para mudanças explícitas de
fonte. Uma máquina de estados possui as transições entre seus estados. Ambos reutilizam
`AnimationTransition`, cada um na sua fronteira de composição, sem duplicar o mesmo fade.
A transição possui a origem, seu progresso e sua política de mistura; não existe um segundo
proprietário do playback de destino.

Uma montage possui sua rota e seu envelope. Ela reutiliza amostragem e percursos de
sequência, mas não precisa herdar de `AnimationPlayback`: seções e interrupções têm regras
próprias. Ela não recebe uma referência de volta para `AnimationLayer`.

A camada coordena fonte e montage, preservando o avanço da fonte sob a ação. Pode manter
`Play`, `PlayMontage` e `StopMontage` como métodos de conveniência que delegam aos
componentes. Seu código principal deve tornar a ordem da avaliação legível, sem conter
os algoritmos de percurso de seções ou de ativação de notifies.

`AnimationInstance` recebe skeleton e pose de referência uma vez. A pose de referência pode
ser específica da mesh; não substituí-la silenciosamente pelo bind pose do skeleton.
O contexto deve ter ownership definido, sem spans apontando para memória temporária.

A mesh permanece fixa durante a vida de `SkeletalMeshInstance`. O renderer continua
consumindo a mesma pose em espaço do modelo. A nova instância não possui geometria,
`GeometryPool`, buffers, comandos Vulkan ou responsabilidades de deformação da GPU.

## 7. Uma atualização, com publicação atômica

A entrada que avança relógios será `AnimationInstance::Update(deltaSeconds)`.
`SkeletalMeshInstance::Update` pode continuar delegando a ela por conveniência. Esses dois
caminhos são alternativas para o mesmo avanço, não duas etapas a chamar no mesmo frame.

No desenho final, `AnimationInstance` possui `AnimationParameters`, camadas e, opcionalmente,
a referência ao asset `AnimationController` que definiu sua configuração. As máquinas de
estados executam como fontes dentro dos playbacks. O controller não mantém outra coleção
de estados atuais nem chama `Update` na instância.

O fluxo interno de um frame fica:

1. Capturar uma visão coerente dos parâmetros para a avaliação.
2. Avaliar fontes, incluindo suas decisões de estados, e preparar percursos sem alterar o
   estado publicado.
3. Amostrar e misturar fontes, transições e montages.
4. Avaliar notifies e extrair root motion usando os percursos já produzidos.
5. Compor Base, aplicar seu bloqueio de root e depois compor as demais camadas em ordem.
6. Validar e construir as matrizes da pose em espaço do modelo.
7. Publicar pose, eventos, movimento e próximos estados somente se tudo tiver sucesso.

Usar `SAnimationFrame` para o resultado publicado: pose local, pose em espaço do modelo,
notifies e root motion. Próximos relógios e estados ficam nos resultados internos dos
componentes, não misturados com o resultado público.

Na implementação, `EvaluateFrame` e `ApplyFrame` podem expressar a preparação e publicação
internas. Não expor um protocolo público de prepare/commit que o usuário precise executar.
`ApplyFrame` só move resultados já validados; não faz consultas ou operações que possam
falhar depois que o estado começou a ser publicado.

Uma falha não pode avançar decisões de estados, relógios, rota de montage ou identidades de
notifies. Eventos e encerramentos pendentes precisam continuar disponíveis para retry.
Essa ampliação da atomicidade das decisões de estados é uma correção comportamental
identificada; deve ser entregue em um commit próprio, com testes específicos.

Preferir buffers temporários reutilizáveis por instância e estados candidatos pequenos.
Não obter atomicidade copiando todas as camadas, assets e poses em cada frame. Reservar
capacidade para o uso comum e medir alocações antes de afirmar que o caminho não aloca.

## 8. API de uso pretendida

Exemplo ilustrativo da API final, depois das etapas de migração:

```cpp
auto& animation = meshInstance.GetAnimation();
animation.SetController(controller);

animation.GetParameters().SetFloat("Forward", speed);
animation.GetLayer("UpperBody")->PlayMontage(combo);

meshInstance.Update(deltaSeconds);

for (const auto& event : animation.GetNotifyEvents())
{
    // Read event.GetName(), event.Type, and event.GetPayload<T>().
}
```

O exemplo omite o tratamento dos retornos para mostrar a navegação. `SetController` deve
validar e preparar suas camadas antes de substituir o estado anterior. O uso manual sem
controller continua disponível através de `animation.GetBaseLayer().Play(sequence)`.

Cada personagem possui sua própria `AnimationInstance` e seus componentes mutáveis.
O asset `AnimationController` pode ser compartilhado. A criação de instâncias de fontes
é polimórfica; somente uma fonte de máquina de estados cria o runtime correspondente.

## 9. Mapeamento da implementação atual

| Código atual | Destino proposto |
| --- | --- |
| `SPlayback` e dispatch repetido de sequência/blend space | `AnimationSource`, sua instância e a fachada `AnimationPlayback` |
| `SamplePlayback`, `AdvancePlayback`, duração e sincronização | Implementação concreta de sequência ou blend space |
| `Sequence`, `BlendSpace` e `StateMachine` opcionais nas definições | Uma referência `AnimationSource` e seus bindings |
| `STransition`, blend de origem/destino, captura de pose em interrupções | `AnimationTransition` |
| `SMontagePlayback`, cursores, links e percurso de seções, envelopes | `AnimationMontagePlayback` |
| `SActiveNotify`, `SNotifyEvaluation`, IDs e encerramentos pendentes | `AnimationNotifyTracker` |
| Política de emissão de eventos | O componente que possui cada composição, sem inspeção dos tipos filhos |
| `BlendPose` e operações override/additive | Funções de `AnimationPose` |
| `ExtractPlaybackRootMotion`, `ExtractSourceRootMotion` e combinação dos trechos | Funções de root motion, consumindo percursos e envelopes |
| `LockRootMotion` e conversões específicas da referência do root | Avaliação da `AnimationInstance` com contexto fixo |
| `SkeletalMeshInstance::ComposePose`, atualização das camadas e agregação de eventos | `AnimationInstance` |
| `AnimationControllerInstance::Matches` e seleção de transição | `StateMachineInstance`, implementação de `AnimationSourceInstance` |
| Parâmetros e getters/setters do controller | `AnimationParameters`, pertencente a `AnimationInstance` |
| Compilação de nomes de parâmetros, estados e seções | Fábricas dos assets e helpers internos de compilação |
| `BlendSpace::FindSamples` chamado por cada operação | Uma resolução de contribuições reutilizada pela avaliação |

Resolver nomes em índices durante a criação do controller. Manter nomes na API de
autoria, nos diagnósticos e nos eventos. As máquinas de estados não devem procurar uma
camada pelo nome para cada transição.

Evitar `friend` como mecanismo geral de integração. Cada componente recebe contextos e
resultados pequenos. Não transferir o conjunto de membros privados de `AnimationLayer`
para outra classe e continuar acessando-o diretamente.

## 10. Sequência de implementação

Cada etapa deve ser um commit revisável, com comportamento preservado salvo onde houver
uma correção declarada. Uma etapa só avança com seus testes e integração funcionando.

### Etapa 1 — Contratos, validação e referência de comportamento

- Definir `AnimationSource` e `AnimationSourceInstance`, seus resultados e contexto.
- Definir quais invariantes cada entidade valida e em qual fronteira.
- Estabelecer o par único `.h`/`.cpp` para cada componente extraído.
- Registrar a saída atual das cenas de sequência, blend space, camadas e combo.
- Registrar baseline de tempo de CPU e alocações para múltiplos personagens, com a mesma
  carga e configurações antes/depois.
- Documentar unidades, política de notifies, pausa, seek, interrupção e root motion.
- Classificar a falha preexistente do teste de validação do controller separadamente.
- Acrescentar cobertura para falha após uma transição proposta pelo controller, incluindo
  estado, pose, relógios e eventos, antes de corrigir a lacuna de atomicidade.

Entrega: contratos pequenos e verificações de regressão relevantes, sem reorganizar tudo.

### Etapa 2 — Extrair `AnimationPlayback`

- Implementar `AnimationPlayback` sobre uma única referência `AnimationSource`.
- Implementar as fontes de sequência e blend space com runtimes próprios.
- Extrair avanço e amostragem com unidades claras e validação local.
- Substituir as alternativas opcionais por uma fonte e bindings comuns.
- Introduzir o resultado de contribuições resolvidas e o percurso temporal compartilhado.
- Manter os métodos públicos de `AnimationLayer` delegando ao componente.

Entrega: reproduzir e testar sequência e blend space pela mesma API, sem construir uma
camada, controller, mesh renderizável ou contexto gráfico. A máquina será integrada a esse
mesmo contrato na etapa 6.

### Etapa 3 — Extrair transições e operações de pose

- Mover relógio do cross-fade e origem anterior/congelada para `AnimationTransition`.
- Extrair blend, override e additive para operações de pose independentes.
- Preservar interrupções, transição de duração zero e continuidade da fonte.

Entrega: testes isolados de transições e composição; a camada apenas solicita e aplica
esses resultados.

### Etapa 4 — Extrair `AnimationMontagePlayback`

- Mover seções, links locais, jumps, envelope e interrupção para um componente próprio.
- Emitir os percursos de seções uma vez e reutilizá-los nos consumidores.
- Preservar fonte contínua, retorno suave do combo e tempos de entrada/saída.

Entrega: testes de rotas e seções independentes de camadas; a integração visual continua
com o mesmo comportamento.

### Etapa 5 — Isolar notifies e extração de movimento

- Extrair `AnimationNotifyTracker` com ativações e cancelamentos autocontidos.
- Remover reconstrução dos relógios pelos consumidores do percurso.
- Centralizar limite de eventos da instância em um contexto de avaliação por frame,
  preservando a exceção dos encerramentos de cancelamento pendentes.
- Separar extração e combinação de root motion da seleção de fontes.

Entrega: notifies e movimento testáveis com percursos pequenos fabricados nos testes,
além da cobertura de integração com sequências e montages.

### Etapa 6 — Isolar decisões do controller

- Extrair o asset `StateMachine` como `AnimationSource` e sua instância concreta.
- Mover parâmetros para `AnimationParameters`, compartilhado pelas fontes do personagem.
- Fazer máquinas produzirem resultados pelo mesmo contrato de sequência e blend space.
- Preparar decisões candidatas, sem mutar imediatamente camadas ou estados publicados.
- Corrigir, em commit específico, a atomicidade de decisões quando a avaliação falha.

Entrega: testar prioridade, condições e conclusão de estados sem uma mesh ou renderer.

### Etapa 7 — Extrair `AnimationInstance` e finalizar a API

- Mover o ciclo completo de avaliação, camadas e resultado final para `AnimationInstance`.
- Aplicar o asset do controller como configuração de parâmetros, fontes e camadas.
- Remover o runtime redundante de `AnimationControllerInstance` e a segunda cadeia de
  atualização, após migrar seus consumidores.
- Fazer `SkeletalMeshInstance` delegar e manter o contrato com o renderer.
- Migrar Dissolve e os testes de integração; remover wrappers temporários redundantes.
- Revisar nomes, ownership, includes e exportação das novas APIs.

Entrega: uma entrada de atualização por personagem e uma dependência clara entre mesh,
instância, camadas e componentes.

## 11. Critérios de aceite

- É possível explicar cada componente sem listar todas as features de animação.
- `AnimationLayer` não implementa percurso de seções, manutenção de ativações ou resolução
  geométrica de blend spaces.
- Sequência, blend space e máquina de estados entram pela mesma API de `AnimationSource`.
- Não existem testes repetidos de tipo, casts ou visitantes nos consumidores das fontes.
- A fonte e a unidade do relógio são explícitas; alternativas inválidas não são representadas
  por dois ponteiros preenchidos simultaneamente.
- Cada regra de validade pertence à sua entidade e é reutilizada; assets imutáveis não são
  revalidados integralmente por frame.
- Cada classe tem seu par único `.h`/`.cpp`. Nenhum segundo `.cpp` implementa métodos
  daquele header.
- Pose, notifies e root motion usam o mesmo avanço, com políticas de seleção distintas
  onde isso é necessário.
- Um frame com erro preserva também as decisões das fontes compostas, não apenas a pose final.
- A mesh fica fixa e a pose de referência específica da mesh é preservada.
- Não há regressão no retorno do combo, nas seções de montage nem no blending aditivo.
- Notifies mantêm payload tipado, ordem, lifetime e cancelamentos em pausa e delta zero.
- A resolução das contribuições não é repetida para cada consumidor da mesma reprodução.
- Os testes de animação podem rodar sem inicializar Vulkan; a integração de skinning mantém
  seus testes de GPU.
- macOS compila e executa os testes relevantes. As novas APIs públicas com símbolos fora
  do header usam `ELIXIR_API`; CI de Windows valida compilação e linking.
- Sem exceptions ou `static_assert` novos no Engine; erros recuperáveis retornam com
  segurança e usam `EE_CORE_ERROR`.
- Não alterar formatação fora dos trechos necessários ao refactor.

## 12. Organização dos arquivos

Exemplos de pares finais, cada um contendo toda a implementação do seu header:

```text
AnimationSource.h                  AnimationSource.cpp
AnimationSourceInstance.h          AnimationSourceInstance.cpp
AnimationPlayback.h                AnimationPlayback.cpp
Sequence.h                Sequence.cpp
SequencePlayback.h        SequencePlayback.cpp
BlendSpace.h                       BlendSpace.cpp
BlendSpacePlayback.h               BlendSpacePlayback.cpp
StateMachine.h            StateMachine.cpp
StateMachineInstance.h    StateMachineInstance.cpp
AnimationTransition.h              AnimationTransition.cpp
AnimationMontagePlayback.h         AnimationMontagePlayback.cpp
AnimationNotifyTracker.h           AnimationNotifyTracker.cpp
AnimationParameters.h              AnimationParameters.cpp
AnimationLayer.h                   AnimationLayer.cpp
AnimationInstance.h                AnimationInstance.cpp
```

As interfaces também seguem o par único: seus destrutores virtuais e demais símbolos fora
do header ficam somente no `.cpp` correspondente, com exportação conforme a fronteira da
DLL. Templates permanecem no header quando necessário pela linguagem.

`AnimationLayerMontage.cpp`, `AnimationLayerNotifies.cpp` e `SequenceNotifies.cpp`
deixam de existir ao final da migração. Métodos ainda pertencentes à sequência voltam para
`Sequence.cpp`; o que pertencer a novos componentes vai para os pares próprios.
Helpers privados de um módulo ficam no seu `.cpp`. Um módulo especializado com header
próprio não pode ser um pretexto para implementar métodos de outro header.

## 13. Extensões futuras

Curvas podem integrar o resultado amostrado. Inertialization pode ser outra política de
transição. IK e constraints podem consumir a pose composta antes da publicação final.
Retargeting pode ser um estágio explícito de conversão de pose. Essas integrações devem
respeitar os limites propostos; não implementar essas features como parte deste refactor.

Não incluir culling, foot IK, terreno, novas regras de blending de root motion, serialização
genérica de payload ou alteração do renderer nesta sequência. As branches posteriores
com outros recursos exigem revisão dos pontos de integração quando forem incorporadas;
este plano descreve o código efetivamente presente na revisão indicada.

## 14. Referências de arquitetura

A Unreal expõe conceitos separados para reprodução de sequência e estado de montage:
[FAnimNode_SequencePlayer](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FAnimNode_SequencePlayer)
e [FAnimMontageInstance](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FAnimMontageInstance).
Isso serve como referência de separação de conceitos, não como um modelo a copiar classe
por classe. A proposta deste documento é específica para os limites e requisitos do Elixir.

A documentação de [Sync Groups](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-sync-groups-in-unreal-engine)
também distingue sincronização de ciclos e prioridade para emissão de notifies. O Elixir
deve explicitar suas próprias políticas e preservar as atuais durante a extração dos
componentes; não assumir que todos os consumidores usam os mesmos pesos ou a mesma fonte.
