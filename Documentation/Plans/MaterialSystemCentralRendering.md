# MaterialSystem: renderização central e preparação assíncrona

## Relação com o plano inicial

Este documento consolida e substitui as decisões arquiteturais do plano
`MaterialSystem.md` sem alterar o arquivo original. Ele incorpora as decisões
tomadas depois da primeira revisão:

- consumidores submetem somente `MaterialInstance`;
- `MaterialHandle`, aquisição explícita e `MaterialRenderProxy` não fazem parte
  da API dos consumidores;
- `MaterialRenderScene` é inicialmente a unidade de submissão;
- cache, snapshots, filas de preparação e proxies são internos ao módulo de
  materiais;
- materiais novos ou desatualizados são preparados em worker threads;
- a preparação inicial pode ser síncrona na primeira versão;
- múltiplos frames em voo e um serial gráfico global fazem parte da base da
  implementação;
- meshes não fazem parte do escopo desta alteração;
- o `Materials::Rendering::Renderer` existente continua sendo o único renderer
  interno de materiais.

## Objetivo

Transformar `MaterialSystem` no ponto central de toda renderização baseada em
materiais da aplicação.

`Application` possui uma única instância de `MaterialSystem`. Aether e outros
produtores descrevem o que precisa ser desenhado por meio de
`MaterialRenderScene`, mas não preparam materiais, não criam tabelas de frame e
não executam passes de materiais.

Todo o estado específico de materiais deve permanecer no módulo de materiais:

- instâncias usadas por submissões;
- snapshots imutáveis;
- cache de materiais compilados;
- cache de instâncias preparadas;
- estado de preparação assíncrona;
- `MaterialRenderProxy`;
- `FrameTable`;
- texture registry;
- buffers por frame;
- batches preparados;
- shaders e pipelines.

O consumidor fornece uma `MaterialInstance`, ponto. O módulo decide se já existe
uma representação preparada, se ela está desatualizada ou se uma nova preparação
deve ser agendada.

## Fora do escopo

Esta alteração não inclui:

- sistema de meshes;
- registro persistente de primitives;
- culling, LOD ou visibility de meshes;
- passes de surface, depth, shadow ou translucency de meshes;
- render graph;
- preparação completamente assíncrona já na primeira etapa;
- mudança geral do modelo de threading de `Material` e `MaterialInstance`.

A arquitetura deve permitir a adição posterior de um sistema de meshes, mas
nenhuma API de primitives deve ser adicionada ao `MaterialSystem` neste trabalho.

## Problemas do fluxo atual

Atualmente, `Aether::Rendering::Renderer`:

1. constrói uma `MaterialRenderScene` exclusiva para partículas;
2. chama `MaterialSystem::PrepareFrame`;
3. inicia o rendering scope;
4. chama `MaterialSystem::Render`;
5. encerra o rendering scope.

Esse fluxo torna o Aether responsável pelo ciclo de renderização de materiais e
expõe a separação entre preparação e execução. Também associa o frame de
materiais ao serial de publicação do Aether, que não representa o frame gráfico
global.

O estado atual mantém somente um `SPreparedFrame` e um único buffer de dados de
materiais. Isso não representa explicitamente os múltiplos frames em voo
suportados pelo `GraphicsContext`.

## Invariantes arquiteturais

- Existe um único `MaterialSystem` por `Application`.
- Somente `Application` controla o início e a execução do frame de materiais.
- Produtores somente constroem e submetem `MaterialRenderScene`.
- Uma submissão contém `MaterialInstance`, nunca `MaterialRenderProxy`.
- Cache, handles internos, snapshots e proxies não são expostos aos produtores.
- `PrepareFrame` não faz parte da API pública.
- O estado preparado nunca é retornado ao consumidor.
- O frame de materiais usa frame index e serial do domínio gráfico.
- Cada frame em voo possui armazenamento de material que não pode ser
  sobrescrito enquanto estiver em uso pela GPU.
- Preparação assíncrona nunca bloqueia a renderização de um frame normal.
- Um resultado assíncrono obsoleto nunca substitui uma revisão mais recente.
- O renderer interno não conhece Aether, `Application` ou qualquer produtor.
- Aether não inicia nem encerra rendering scopes de materiais.
- Falha de preparação de um material não interrompe os demais draws.

## Responsabilidades

### Application

`Application`:

- cria e possui `MaterialSystem`;
- inicia o frame de materiais dentro do callback da render thread;
- permite que a implementação de `Render` e seus subsistemas submetam cenas;
- solicita a execução do frame depois de todas as submissões;
- mantém a ordem entre materiais, GUI e outros sistemas de rendering.

### MaterialSystem

`MaterialSystem`:

- recebe `MaterialRenderScene`;
- identifica todas as `MaterialInstance` requeridas no frame;
- consulta o cache interno de instâncias;
- captura snapshots para entradas ausentes ou desatualizadas;
- prepara sincronamente os materiais iniciais enquanto essa política estiver
  habilitada;
- agenda preparações posteriores no worker pool;
- consome resultados concluídos sem esperar pelos workers;
- seleciona proxy pronto, último proxy válido ou fallback;
- constrói a `FrameTable` global do frame;
- agrupa e ordena os itens preparados;
- cria e enfileira o command buffer gráfico de materiais;
- delega programa, shader, descriptors e pipeline ao renderer interno;
- mantém métricas globais do frame de materiais.

### Materials::Rendering::Renderer

O renderer interno existente:

- mantém o cache de compilação ou utiliza o serviço interno responsável por ele;
- transforma dados compilados e snapshots de instância em
  `MaterialRenderProxy`;
- encontra o programa correspondente ao pass;
- prepara descriptor bindings;
- cria e reutiliza graphics pipelines;
- fornece shader e pipeline para a emissão dos draws.

Ele não possui a fila global do frame e não controla o ciclo público de
submissão.

### Aether

Aether:

- publica instâncias e dados imutáveis de simulação;
- grava sua simulação e suas barreiras de compute;
- mantém os recursos de partículas vivos durante a submissão;
- constrói uma `MaterialRenderScene` com geometrias e draws de partículas;
- submete essa cena ao `MaterialSystem`;
- mantém métricas específicas de simulação e de itens submetidos.

Aether não:

- resolve `MaterialInstance` em proxy;
- prepara a tabela de materiais;
- chama preparação ou execução de materiais;
- escolhe o shader ou graphics pipeline final;
- inicia ou encerra o rendering scope gráfico dos materiais.

## Fluxo do frame

```text
Application render callback
    |
    +-- MaterialSystem.BeginFrame()
    |
    +-- Aether simulation
    |      |
    |      +-- records compute work and barriers
    |      +-- enqueues compute command buffer
    |      +-- builds MaterialRenderScene
    |      +-- MaterialSystem.Submit(scene)
    |
    +-- other producers submit scenes
    |
    +-- MaterialSystem.RenderFrame()
    |      |
    |      +-- publishes completed preparation jobs
    |      +-- finds required MaterialInstance objects
    |      +-- schedules missing or stale entries
    |      +-- selects ready, previous, or fallback proxies
    |      +-- builds the frame material table
    |      +-- builds and sorts batches
    |      +-- records the graphics command buffer
    |      +-- enqueues the graphics command buffer
    |
    +-- GUI rendering
```

No modelo atual de command buffers secundários, a simulação do Aether deve ser
enfileirada antes do command buffer gráfico criado por `MaterialSystem`. A ordem
de enfileiramento preserva a dependência enquanto a gravação ocorre
sequencialmente no callback da render thread.

Se a gravação de command buffers se tornar paralela, a ordem implícita da fila
não será suficiente. Essa evolução deve usar dependências explícitas ou um
render graph.

## API pública proposta

### MaterialRenderScene

`MaterialRenderScene` continua armazenando geometrias separadamente dos draws.
Isso preserva o compartilhamento atual de buffers e bindings entre vários itens.

```cpp
namespace Elixir::Materials::Rendering
{
    /**
     * @brief Describes one material-backed draw for the current frame.
     */
    struct SRenderItem
    {
        /** Material pass used by the draw. */
        EMaterialPass Pass = EMaterialPass::ParticleSprite;

        /** Material instance used by the draw. */
        Ref<MaterialInstance> Material;

        /** Index of geometry stored in the containing scene. */
        uint32_t GeometryIndex = UINT32_MAX;

        /** Push constants applied before the draw. */
        SMaterialPushConstants PushConstants;

        /** Draw range for the item. */
        SDrawCommand Draw;
    };
}
```

O consumidor não chama `AcquireMaterial`, não recebe um `MaterialHandle` e não
inclui headers de `MaterialRenderProxy`.

### MaterialSystem

```cpp
namespace Elixir::Materials
{
    /**
     * @brief Coordinates material rendering for the application.
     *
     * The system collects material scenes, prepares material instances, builds
     * shared frame resources, and records material draw commands.
     */
    class ELIXIR_API MaterialSystem final
        : public Rendering::MaterialResolver
    {
    public:
        /**
         * @brief Starts material submission for the current graphics frame.
         */
        void BeginFrame();

        /**
         * @brief Adds a material scene to the current frame.
         */
        void Submit(Rendering::MaterialRenderScene Scene);

        /**
         * @brief Prepares and renders all material scenes submitted for the frame.
         */
        SMaterialRenderResult RenderFrame();

        /**
         * @brief Resolves an instance for compatibility with existing consumers.
         *
         * New render producers must submit MaterialInstance through
         * MaterialRenderScene instead of requesting a render proxy.
         */
        Ref<const Rendering::MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& Instance
        ) override;
    };
}
```

`MaterialResolver` permanece durante a migração porque o caminho atual de
compilação do Aether depende dele. Depois que todos os consumidores enviarem
`MaterialInstance`, a necessidade de manter esse contrato público deve ser
reavaliada em uma alteração separada. A remoção não é necessária para concluir
este plano.

## Integração com Application

O ciclo deve ser inserido ao redor da chamada virtual de rendering:

```cpp
m_GraphicsContext->RenderFrame([this, FrameTime]()
{
    m_MaterialSystem->BeginFrame();

    Render(FrameTime);

    m_MaterialSystem->RenderFrame();
    m_GUIManager->Render();
});
```

`BeginFrame` obtém do `GraphicsContext`:

- frame number global;
- frame index;
- número de frames em voo;
- color target principal;
- depth-stencil target;
- render extent.

Esses dados não precisam ser fornecidos pelo Aether nem associados ao serial de
uma publicação do Aether.

## Submissão de múltiplas cenas

Cada `MaterialRenderScene` possui índices locais de geometria. Inicialmente, o
sistema deve armazenar as cenas sem combiná-las fisicamente:

```cpp
struct SFrameSlot
{
    uint64_t FrameSerial = 0;
    uint32_t FrameIndex = 0;

    std::vector<Rendering::MaterialRenderScene> Scenes;
    std::vector<SPreparedRenderItem> PreparedItems;
    std::vector<SMaterialBatch> Batches;

    Ref<Rendering::FrameTable> MaterialTable;
    Ref<DynamicStorageBuffer> MaterialBuffer;
};
```

Uma geometria é identificada internamente por cena e índice:

```cpp
struct SFrameGeometryKey
{
    uint32_t SceneIndex = UINT32_MAX;
    uint32_t GeometryIndex = UINT32_MAX;

    bool operator==(const SFrameGeometryKey&) const = default;
};
```

Isso evita:

- remapeamento de índices durante a submissão;
- cópia de `SRenderGeometry` em cada draw;
- colisões entre índices locais de produtores diferentes;
- perda do compartilhamento de geometria já usado pelo Aether.

A `FrameTable` percorre todas as cenas e continua sendo única para o frame.

Uma operação futura de flatten ou merge só deve ser adicionada se medições
mostrarem benefício.

## Estado interno do frame

`MaterialSystem` mantém um slot para cada frame em voo:

```cpp
std::vector<SFrameSlot> m_Frames;
SFrameSlot* m_CurrentFrame = nullptr;
```

O slot é selecionado por `GraphicsContext::GetFrameIndex()` e identificado por
`GraphicsContext::GetFrameNumber()`.

O ciclo do slot é:

```text
Available --> Collecting --> Preparing --> Recording --> Submitted
    ^                                                   |
    +------------- GPU completion / slot reuse ---------+
```

`BeginFrame`:

1. seleciona o slot correspondente ao frame index;
2. confirma que o `GraphicsContext` já tornou o slot seguro para reutilização;
3. limpa cenas, itens preparados e batches anteriores;
4. atualiza frame serial e frame index;
5. inicia o `TextureRegistry` com o serial gráfico global;
6. muda o estado para `Collecting`.

`Submit` só aceita cenas durante `Collecting`.

`RenderFrame` sela as submissões, prepara os materiais disponíveis, grava os
comandos e muda o slot para `Submitted`.

## Recursos GPU por frame em voo

Não basta armazenar somente `FrameTable` e batches por slot. O buffer GPU que
recebe `SMaterialFrameData` também não pode ser sobrescrito enquanto outro frame
o utiliza.

A primeira implementação deve escolher uma destas estratégias:

1. um `DynamicStorageBuffer` por frame em voo; ou
2. um único buffer em formato ring, com uma região e offset por frame.

Um buffer por frame é a opção inicial recomendada por ser mais simples e menos
propensa a erro.

O renderer interno atualmente recebe um único frame buffer no construtor. Ele
deve passar a receber o buffer do slot na preparação do pass ou usar um pequeno
objeto de recursos do frame:

```cpp
struct SMaterialFrameResources
{
    Ref<DynamicStorageBuffer> MaterialBuffer;
    Ref<Rendering::FrameTable> MaterialTable;
};
```

O buffer e a tabela permanecem vivos pelo menos até que o slot possa ser
reutilizado.

## Cache interno de materiais

O cache possui duas camadas conceituais.

### Cache de material compilado

Chave lógica:

```text
Material identity + Material revision
```

Resultado:

```cpp
Ref<const SCompiledMaterial>
```

O `CompilationCache` existente já cobre parte dessa responsabilidade e deve ser
reutilizado ou adaptado, não duplicado.

### Cache de instância preparada

Chave lógica:

```text
MaterialInstance identity
+ MaterialInstance revision
+ parent Material revision
```

Resultado:

```cpp
Ref<const MaterialRenderProxy>
```

Estrutura interna proposta:

```cpp
enum class EMaterialPreparationState : uint8_t
{
    Pending,
    Preparing,
    Ready,
    Refreshing,
    Failed,
};

struct SMaterialInstanceCacheEntry
{
    std::weak_ptr<MaterialInstance> Instance;
    Ref<const MaterialRenderProxy> Proxy;

    uint32_t PreparedInstanceRevision = 0;
    uint32_t PreparedMaterialRevision = 0;
    uint64_t JobGeneration = 0;

    EMaterialPreparationState State =
        EMaterialPreparationState::Pending;

    std::string Diagnostics;
};
```

O mapa pode ser indexado inicialmente pelo endereço da instância. A entrada deve
conter uma referência fraca e uma geração para evitar aceitar resultados de uma
instância destruída ou de um job anterior. Entradas expiradas devem ser removidas
periodicamente.

O cache não deve manter todas as `MaterialInstance` vivas indefinidamente. As
`MaterialRenderScene` do frame mantêm referências fortes enquanto os draws ainda
precisam das instâncias.

## Detecção de entradas pendentes

Durante `RenderFrame`, o sistema percorre todas as cenas e deduplica as instâncias
requeridas por identidade.

Para cada instância:

| Condição | Ação |
| --- | --- |
| Não existe no cache | Capturar snapshot e marcar `Pending` |
| Revisões coincidem | Usar proxy `Ready` |
| Revisão mudou e há proxy | Manter proxy anterior e marcar `Refreshing` |
| Revisão mudou e não há proxy | Marcar `Pending` |
| Falha na mesma revisão | Não reagendar automaticamente todo frame |
| Falha e revisão mudou | Capturar novo snapshot e reagendar |
| Instância expirou | Remover entrada quando não houver job válido |

Cada instância deve gerar no máximo um job para uma combinação de revisões. A
deduplicação acontece antes do envio ao worker pool.

## Snapshot imutável

Workers não podem ler diretamente `Material`, `MaterialGraph` ou
`MaterialInstance`, pois esses objetos são mutáveis e atualmente não possuem um
contrato de leitura concorrente.

Antes de agendar um job, o módulo captura um snapshot imutável:

```cpp
struct SMaterialDefinitionSnapshot
{
    std::string Name;
    MaterialGraph Graph;
    std::unordered_map<std::string, SMaterialParameterDefinition> Parameters;
    uint32_t UsageMask = 0;
    uint32_t Revision = 0;
};

struct SMaterialInstanceSnapshot
{
    const MaterialInstance* Identity = nullptr;
    uint64_t JobGeneration = 0;

    uint32_t InstanceRevision = 0;
    uint32_t MaterialRevision = 0;

    SMaterialDefinitionSnapshot Material;
    std::unordered_map<std::string, SMaterialParameter> ResolvedParameters;
};
```

Os tipos concretos podem ser ajustados para evitar cópias desnecessárias. O
requisito é que o worker receba ownership independente e não consulte objetos
mutáveis depois que o job começa.

A captura também precisa ser segura em relação aos setters. Opções aceitáveis:

1. mutex interno usado somente durante mutação e captura;
2. estado copy-on-write publicado atomicamente;
3. fila de alterações aplicada por uma única thread proprietária.

Para a primeira implementação assíncrona, um mutex de curta duração durante a
captura é aceitável. Copy-on-write pode ser adotado depois caso o lock apareça em
profiles.

## Pipeline de preparação assíncrona

```text
Material render thread                 Worker pool
----------------------                 -----------
collect required instances
detect missing or stale entry
capture immutable snapshot
mark job generation
enqueue request ---------------------> validate graph
continue without waiting               generate material code
use previous proxy or fallback          compile CPU artifacts
                                         |
consume completed result <--------------+
validate identity, revisions and generation
finalize render resources when required
publish immutable proxy
```

Estados detalhados:

```text
Missing --> Pending --> Preparing --> Ready
                         |
                         +-----------> Failed

Ready -- revision changed --> Refreshing --> Ready(new revision)
                                  |
                                  +--------> Ready(previous revision) + diagnostics
```

Os workers enviam resultados para uma completion queue. Somente a thread que
possui o frame de materiais publica resultados no cache principal.

O `Executor` existente deve ser usado para enviar trabalhos ao worker pool. A
renderização normal nunca chama `Wait`, espera uma future ou bloqueia por uma
preparação pendente.

## Separação entre CPU e GPU

O `Compiler::Compile` atual valida o graph, gera HLSL, chama o compilador e carrega
programas por meio de `ShaderLoader`. Não se deve assumir que todo esse caminho é
seguro em worker threads apenas porque `CompilationCache` usa mutex.

A implementação assíncrona deve ser dividida conceitualmente em:

### Worker-safe preparation

- validação do snapshot;
- geração de código;
- criação do layout de parâmetros;
- compilação para artefatos intermediários;
- criação dos dados resolvidos da instância que não dependem do RHI.

### Render-safe finalization

- consumo dos artefatos concluídos;
- criação ou publicação de shaders e recursos que tenham restrição de thread;
- criação do `MaterialRenderProxy` imutável;
- atualização do cache principal;
- registro de diagnósticos.

Se `ShaderLoader` e o backend forem comprovadamente seguros para uso concorrente,
parte ou toda a finalização poderá ocorrer no worker. Essa capacidade precisa ser
validada no código e nas plataformas suportadas antes de ser habilitada.

A resolução de texturas em índices bindless permanece no frame de rendering,
pois depende do `TextureRegistry` e do momento em que atualizações de descriptors
se tornam visíveis.

## Publicação de resultados

Um resultado de worker contém toda a informação necessária para validar sua
atualidade:

```cpp
struct SMaterialPreparationResult
{
    const MaterialInstance* Identity = nullptr;
    uint64_t JobGeneration = 0;

    uint32_t InstanceRevision = 0;
    uint32_t MaterialRevision = 0;

    Ref<const MaterialRenderProxy> Proxy;
    std::string Diagnostics;
};
```

Ao consumir o resultado, o sistema confirma:

- a entrada ainda existe;
- a referência fraca ainda identifica o mesmo objeto;
- `JobGeneration` corresponde ao job atual;
- a revisão da instância ainda é a mesma;
- a revisão do parent material ainda é a mesma.

Se qualquer verificação falhar, o resultado é descartado. Caso a instância ainda
seja necessária, uma nova revisão é marcada como pendente.

O descarte de resultado obsoleto é comportamento normal, não erro.

## Política de fallback e atualização

| Estado | Comportamento no frame |
| --- | --- |
| `Ready` e revisões atuais | Usar o proxy atual |
| Material novo em preparação | Usar material fallback |
| Refresh com proxy anterior | Usar o último proxy válido |
| Falha sem proxy anterior | Usar fallback ou ignorar o draw conforme política |
| Falha com proxy anterior | Usar o proxy anterior e registrar diagnóstico |

O fallback deve ser criado e preparado sincronamente durante a inicialização do
`MaterialSystem`. Portanto, ele nunca depende de um job pendente.

Uma falha deve ser armazenada junto às revisões que falharam. O sistema não deve
repetir a mesma compilação em todos os frames. Uma nova tentativa ocorre quando:

- a revisão muda;
- o consumidor solicita explicitamente uma recompilação; ou
- uma política de retry controlada for adicionada futuramente.

## Preparação síncrona inicial

Na primeira versão, os materiais exigidos pelo primeiro frame renderizado podem
ser preparados sincronamente:

```text
First graphics frame
    collect instances
    prepare all missing instances synchronously
    build frame table
    render
```

Depois desse bootstrap:

- materiais novos entram na fila de workers;
- materiais alterados entram em `Refreshing`;
- nenhum frame espera a conclusão desses jobs;
- fallback ou último proxy válido é usado enquanto necessário.

Essa política deve estar isolada em uma condição interna para que possa ser
removida sem alterar a API pública. Uma evolução posterior pode fazer warmup de
materiais conhecidos antes do primeiro frame e eliminar o bloqueio inicial.

A preparação síncrona de shader não garante que uma textura recém-adicionada
esteja visível no descriptor set no mesmo callback. O comportamento de fallback
do `TextureRegistry` continua válido até o frame em que a atualização de
descriptor estiver disponível.

## Construção da cena preparada

`MaterialRenderScene` representa a entrada pública. A execução utiliza uma
representação interna que associa cada item ao proxy escolhido para aquele frame:

```cpp
struct SPreparedRenderItem
{
    const Rendering::SRenderItem* Source = nullptr;
    Ref<const Rendering::MaterialRenderProxy> Material;
    SFrameGeometryKey Geometry;
    uint32_t MaterialIndex = UINT32_MAX;
};
```

O processo é:

1. percorrer os itens de todas as cenas;
2. localizar a entrada de cache da `MaterialInstance`;
3. escolher proxy atual, proxy anterior ou fallback;
4. adicionar o proxy escolhido à `FrameTable`;
5. criar `SPreparedRenderItem` com o índice retornado;
6. agrupar os itens preparados por pass, geometria e programa;
7. ordenar os batches pela política do renderer;
8. preparar pipeline e emitir draws.

Os ponteiros para `SRenderItem` são válidos porque as cenas permanecem imóveis no
slot até a gravação terminar. Se a preparação passar a sobreviver além da
gravação do frame, os itens devem ser armazenados por valor ou por ownership
explícito.

## MaterialFrameTable e TextureRegistry

A tabela é construída uma vez por frame a partir dos proxies efetivamente
selecionados. O mesmo proxy usado por cenas diferentes ocupa uma única entrada.

O `TextureRegistry` usa o frame serial gráfico global, nunca o serial do Aether.
Quando uma textura é registrada durante um callback, ela pode continuar usando o
fallback até a próxima atualização visível de descriptors.

A capacidade inicial de `FrameTable` não deve ser tratada como capacidade máxima
silenciosa. Como o frame passa a agregar todos os produtores, o sistema deve:

- detectar capacidade insuficiente antes do upload;
- aumentar os buffers do slot com uma política definida; ou
- falhar explicitamente com diagnóstico e fallback previsível.

Ignorar uma falha de `FrameTable::Add` e descobrir o material ausente somente
durante batching não é aceitável.

## Command buffer e rendering scope

Para a infraestrutura atual, `MaterialSystem::RenderFrame` deve:

1. obter um command buffer secundário do `GraphicsContext`;
2. iniciar o command buffer com color target, depth target e render area atuais;
3. iniciar o rendering scope;
4. configurar viewport e scissor;
5. executar todos os batches de materiais;
6. encerrar o rendering scope;
7. enfileirar o command buffer.

O sistema obtém os targets diretamente do `GraphicsContext`. O Aether não fornece
nem controla esses recursos.

Um frame sem itens não precisa criar nem enfileirar um command buffer gráfico.

## Métricas

`SMaterialRenderResult` representa o frame inteiro:

```cpp
struct SMaterialRenderResult
{
    uint32_t MaterialCount = 0;
    uint32_t SceneCount = 0;
    uint32_t BatchCount = 0;
    uint32_t DrawCount = 0;
    uint32_t FallbackDrawCount = 0;
    uint32_t PendingMaterialCount = 0;
    uint32_t FailedMaterialCount = 0;
};
```

Métricas do Aether devem representar:

- sistemas simulados;
- partículas processadas;
- itens de materiais submetidos;
- serial da publicação do Aether.

Elas não devem copiar os totais globais de batches e materiais do
`MaterialSystem`, pois outros produtores poderão contribuir para o mesmo frame.

## Threading

### Render/material owner thread

Na primeira implementação, `BeginFrame`, `Submit` e `RenderFrame` são chamados
sequencialmente na render thread. O cache principal e os slots de frame são
possuídos por essa thread.

### Worker threads

Workers recebem apenas snapshots e produzem resultados imutáveis. Eles não:

- modificam slots de frame;
- modificam o cache principal;
- acessam cenas submetidas;
- acessam diretamente `MaterialInstance` ou `Material`;
- atualizam `TextureRegistry`;
- gravam command buffers gráficos.

### Completion queue

A completion queue sincronizada conecta workers à owner thread. O consumo ocorre
no início de `RenderFrame` ou de `BeginFrame`, sem espera bloqueante.

### Shutdown

Durante shutdown:

- novas preparações deixam de ser aceitas;
- jobs existentes devem ser cancelados cooperativamente ou drenados;
- resultados tardios não podem acessar `MaterialSystem` destruído;
- o worker deve capturar dados por valor, nunca `this` cru sem garantia de vida;
- recursos GPU são destruídos somente depois do mecanismo normal de idle e
  retirement do graphics context.

## Migração do Aether

### Estado atual

`SCompiledEmitter` e `Simulation::SRenderItem` mantêm
`Ref<const MaterialRenderProxy>`. O Aether resolve o material durante sua
compilação e publica o proxy no frame imutável.

### Estado desejado

O Aether mantém `Ref<MaterialInstance>` como parte do estado publicado necessário
ao draw. O `Simulation::RenderFrame` continua imutável enquanto retém uma
referência forte para a instância.

O Aether não lê parâmetros da instância. Ele somente transfere a referência para
`MaterialRenderScene`. O `MaterialSystem` captura e prepara o estado necessário.

A validação de compatibilidade entre render mode e material usage pode verificar
o parent `Material` durante a compilação do emitter. A validação definitiva
também ocorre no módulo de materiais antes de criar o batch.

O método atual do Aether pode evoluir para:

```cpp
void Aether::Rendering::Renderer::SubmitRenderItems(
    const Simulation::RenderFrame& Frame,
    const Camera& Camera,
    Materials::MaterialSystem& Materials
);
```

Ou pode retornar uma cena para que `Aether::Manager` faça a submissão:

```cpp
Materials::Rendering::MaterialRenderScene
Aether::Rendering::Renderer::BuildMaterialRenderScene(
    const Simulation::RenderFrame& Frame,
    const Camera& Camera
);
```

A segunda forma mantém o renderer do Aether independente do `MaterialSystem` e é
preferível quando não introduz cópia adicional:

```cpp
auto Scene = Renderer.BuildMaterialRenderScene(*Frame, Camera);
Materials.Submit(std::move(Scene));
```

## Uso do MaterialResolver durante a migração

`MaterialSystem` continua implementando `MaterialResolver` inicialmente para não
quebrar consumidores existentes em uma única mudança.

Depois que o Aether deixar de armazenar proxies:

- localizar outros consumidores de `MaterialResolver`;
- confirmar se o contrato ainda tem uso público;
- se não houver consumidores, tornar a resolução uma operação privada entre
  `MaterialSystem`, cache e renderer interno;
- remover includes de `MaterialRenderProxy` dos módulos externos.

Essa limpeza é posterior e não bloqueia o novo fluxo central.

## Etapas de implementação

### Etapa 1: slots de frame e serial global

- Criar um `SFrameSlot` por frame em voo.
- Criar um material buffer por slot.
- Selecionar o slot por `GraphicsContext::GetFrameIndex()`.
- Usar `GraphicsContext::GetFrameNumber()` como serial gráfico.
- Adaptar o renderer interno para usar os recursos do slot corrente.
- Manter temporariamente a API atual de Aether.

### Etapa 2: submissão central síncrona

- Alterar `MaterialRenderScene::SRenderItem` para receber `MaterialInstance`.
- Adicionar `MaterialSystem::BeginFrame`, `Submit` e `RenderFrame`.
- Armazenar múltiplas cenas por slot sem remapear suas geometrias.
- Criar o cache interno de instâncias.
- Preparar materiais ausentes sincronamente.
- Criar e preparar o fallback durante inicialização.
- Construir `FrameTable` e batches internos.
- Mover rendering scope e emissão de comandos para `MaterialSystem`.
- Inserir o ciclo central no callback de `Application`.

### Etapa 3: migrar o Aether

- Fazer compiled emitter e simulation render item reterem `MaterialInstance`.
- Remover resolução de proxy do caminho de compilação do Aether quando possível.
- Fazer o renderer do Aether apenas construir `MaterialRenderScene`.
- Enfileirar comandos de simulação antes do frame gráfico de materiais.
- Remover chamadas de `PrepareFrame`, `Render`, `BeginRendering` e
  `EndRendering` do Aether.
- Separar métricas de submissão do Aether das métricas globais de materiais.

### Etapa 4: snapshots e jobs assíncronos

- Definir snapshots imutáveis de material e instância.
- Tornar a captura segura em relação a mutações.
- Adicionar estados, geração de job e deduplicação de pendências.
- Enviar preparação para o worker pool do `Executor`.
- Criar a completion queue.
- Publicar resultados somente na owner thread.
- Descartar resultados obsoletos por identidade, revisão e geração.
- Usar fallback ou último proxy válido enquanto um job estiver pendente.
- Armazenar falhas por revisão para evitar recompilação contínua.

### Etapa 5: separar compilação e finalização

- Auditar a thread-safety de `Compiler`, `ShaderLoader`, shader backend e criação
  de recursos gráficos.
- Separar geração e compilação worker-safe da finalização render-safe.
- Medir o custo da finalização na render thread.
- Mover mais trabalho para workers somente quando suportado em macOS e Windows.

### Etapa 6: limpeza da API

- Tornar `PrepareFrame`, batching e execução detalhes privados.
- Remover getters públicos de recursos usados somente pelo renderer interno.
- Reavaliar e eventualmente remover `MaterialResolver` público.
- Remover dependências externas de `MaterialRenderProxy`.
- Atualizar documentação Doxygen e exemplos.

## Validação

### Unidade: cache e revisões

- Primeira consulta cria exatamente uma entrada pendente.
- Submissões repetidas da mesma instância não duplicam jobs.
- Cache hit com revisões iguais reutiliza o mesmo proxy.
- Mudança da instância agenda refresh.
- Mudança do parent material agenda recompilação e refresh.
- Falha na mesma revisão não é reagendada a cada frame.
- Mudança após falha permite nova tentativa.
- Resultado com geração antiga é descartado.
- Resultado de instância expirada é descartado.
- Entrada sem referência viva é removida do cache.

### Unidade: snapshots

- Snapshot contém graph, usages, schema, defaults e overrides da mesma revisão.
- Worker não acessa o objeto mutável depois do agendamento.
- Alteração posterior da instância não modifica um snapshot existente.
- Resultado preserva as revisões usadas pelo job.

### Unidade: frames em voo

- Cada frame index seleciona seu próprio slot e buffer.
- Reutilização limpa apenas o slot seguro.
- Upload de um frame não altera os dados GPU de outro frame em voo.
- Frame serial é global e monotônico.
- Texture readiness usa o serial gráfico, não o serial do Aether.

### Integração: submissão e rendering

- Um frame com uma cena mantém o resultado visual atual do Aether.
- Múltiplas cenas compartilham a mesma `FrameTable`.
- Índices locais de geometria não colidem entre cenas.
- Material repetido ocupa uma única entrada na tabela.
- Frame sem cenas não grava draw commands.
- Cena vazia não impede outras cenas de renderizar.
- Capacidade insuficiente é tratada explicitamente.
- `MaterialCount`, `BatchCount` e `DrawCount` representam o frame global.

### Integração: preparação assíncrona

- Primeiro frame pode preparar materiais sincronamente.
- Material novo depois do bootstrap não bloqueia o frame.
- Material pendente usa fallback.
- Refresh pendente usa último proxy válido.
- Resultado concluído passa a ser usado em um frame posterior.
- Falha de um material não impede outros materiais de renderizar.
- Shutdown com jobs pendentes não acessa objetos destruídos.

### Ordem e sincronização

- Simulação do Aether é executada antes dos draws que consomem seus buffers.
- Barreiras compute-to-graphics continuam válidas após separar os command buffers.
- Material rendering ocorre antes ou depois da GUI conforme a ordem definida.
- Nenhum worker grava command buffer gráfico ou modifica o frame corrente.

### Plataformas

- Compilar e validar comportamento em macOS e Windows.
- Confirmar a thread-safety real dos trechos executados em workers em ambas as
  plataformas.
- Verificar símbolos que cruzam a fronteira da DLL e aplicar `ELIXIR_API` aos
  tipos e funções públicos necessários.
- Confirmar que completion queue, snapshots e resultados não dependem de
  comportamento específico do linker do macOS.
- Executar testes relevantes no CI Windows.
- Validar lifetime de command buffers, buffers por frame e recursos Vulkan.

## Critérios de conclusão

O trabalho deste plano está concluído quando:

1. `Application` controla um único ciclo de materiais por frame;
2. Aether somente simula, constrói e submete `MaterialRenderScene`;
3. consumidores submetem `MaterialInstance` e não conhecem proxies ou handles;
4. `MaterialSystem` mantém cache e estado de preparação internamente;
5. materiais iniciais podem ser preparados sincronamente;
6. preparações posteriores são executadas em workers sem bloquear frames;
7. fallback ou último proxy válido é usado durante preparação;
8. resultados obsoletos são descartados com segurança;
9. slots e buffers por frame em voo são independentes;
10. `FrameTable` é construída uma vez para todas as cenas do frame;
11. serial de textura e frame pertence ao domínio gráfico;
12. o renderer interno existente continua sendo o único executor interno;
13. nenhuma API de meshes ou primitives é adicionada;
14. testes relevantes passam em macOS e Windows.
