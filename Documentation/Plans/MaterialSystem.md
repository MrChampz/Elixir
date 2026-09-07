# MaterialSystem: plano de arquitetura e implementação

## Objetivo

Transformar `MaterialSystem` no ponto central de toda renderização baseada em
materiais da aplicação. Aether, meshes e futuros produtores devem descrever o
que precisa ser desenhado, mas não podem preparar nem executar materiais por
conta própria.

`MaterialSystem` é uma instância única, criada e mantida por `Application`. O
estado preparado do frame deve permanecer dentro do sistema e não deve ser
retornado para outro componente transportar até uma chamada posterior de
renderização.

Este plano não cria outro renderer de materiais. O
`Elixir::Materials::Rendering::Renderer` existente continua sendo o executor
interno responsável por resolver materiais e preparar shaders, bindings e
pipelines.

## Problemas do desenho atual

O fluxo atual permite que `Aether::Rendering::Renderer`:

1. construa uma `MaterialRenderScene` exclusiva para partículas;
2. chame `MaterialSystem::PrepareFrame`;
3. inicie o escopo de renderização;
4. chame `MaterialSystem::Render`;
5. finalize o escopo de renderização.

Esse desenho impede que o mesmo frame de materiais represente conjuntamente
partículas, meshes e outros produtores. Também expõe detalhes que deveriam ser
internos, como a separação entre preparação e execução e a necessidade de
associá-las por um `submissionSerial`.

Se meshes seguirem o mesmo padrão, cada subsistema preparará sua própria tabela
de materiais, controlará sua própria execução e chamará o sistema em momentos
diferentes. Isso elimina a possibilidade de deduplicação global, batching entre
produtores e uma ordenação central dos passes.

## Decisões arquiteturais

### Ownership

- `Application` possui uma única instância de `MaterialSystem`.
- `MaterialSystem` possui o estado de materiais de cada frame em voo.
- `MaterialSystem` possui o `Materials::Rendering::Renderer` existente.
- Aether, meshes e outros subsistemas são produtores de submissões.
- Produtores não controlam o ciclo de vida do frame de materiais.
- Produtores não chamam preparação, batching ou execução de materiais.

### Responsabilidades do MaterialSystem

`MaterialSystem` deve:

- iniciar a coleta de um frame;
- receber draws baseados em materiais de todos os produtores;
- resolver instâncias de materiais em representações próprias para rendering;
- deduplicar materiais e texturas entre todos os draws do frame;
- construir e enviar a `FrameTable`;
- agrupar e ordenar draws por pass, geometria e programa;
- delegar a preparação de shaders, bindings e pipelines ao renderer interno;
- gravar ou coordenar a gravação dos draw commands;
- manter o estado preparado privado;
- expor somente métricas e resultados, nunca um snapshot necessário para uma
  chamada posterior.

### Responsabilidades do renderer interno

O `Elixir::Materials::Rendering::Renderer` existente deve continuar responsável
por:

- compilar ou recuperar materiais renderizáveis;
- resolver `MaterialInstance` em `MaterialRenderProxy`;
- selecionar o programa correspondente ao pass;
- criar e reutilizar graphics pipelines;
- preparar descriptor bindings;
- retornar o shader e o pipeline preparados para execução.

Ele não deve possuir a lista global de draws do frame nem conhecer Aether,
meshes ou `Application`.

### Responsabilidades dos produtores

Aether, meshes e outros produtores devem:

- preparar seus próprios dados de geometria;
- atualizar constant buffers e storage buffers específicos do domínio;
- executar ou agendar trabalho de compute específico do domínio;
- transformar seu estado em submissões genéricas de materiais;
- preservar a ordem necessária entre o trabalho de preparação e os draws
  submetidos.

Eles não devem:

- chamar `MaterialSystem::PrepareFrame`;
- chamar `MaterialSystem::Render`;
- construir uma tabela de materiais;
- escolher o shader ou pipeline final;
- controlar diretamente a execução dos passes de materiais.

## Fluxo desejado

```text
Aether -----------------+
Meshes -----------------+--> material submissions --> MaterialSystem
Terrain ----------------+                              |
Future producers -------+                              +--> resolve materials
                                                       +--> build frame table
                                                       +--> upload frame data
                                                       +--> build and sort batches
                                                       +--> internal Renderer
                                                       +--> GPU draw commands
```

O ciclo público do frame deve ser controlado por `Application`:

```cpp
MaterialSystem.BeginFrame(FrameContext);

Aether.SubmitRenderItems(Camera, MaterialSystem);
MeshRenderer.SubmitRenderItems(Scene, Camera, MaterialSystem);

const auto Result = MaterialSystem.RenderFrame();
```

`BeginFrame` abre a coleta. `RenderFrame` fecha a coleta, prepara os dados e
executa todos os draws submetidos. Não existe objeto preparado retornado por uma
função e posteriormente recebido por outra.

## API pública proposta

Os nomes definitivos podem ser ajustados durante a implementação, mas a
separação de responsabilidades deve permanecer.

```cpp
namespace Elixir::Materials::Rendering
{
    /**
     * @brief Describes the graphics state shared by material draws in one frame.
     */
    struct SMaterialFrameContext
    {
        uint64_t SubmissionSerial = 0;
        Ref<Image> ColorTarget;
        Ref<Image> DepthTarget;
        Extent2D RenderArea;
    };

    /**
     * @brief Describes one material-backed draw.
     */
    struct SMaterialDrawSubmission
    {
        Ref<MaterialInstance> Material;
        EMaterialPass Pass = EMaterialPass::ParticleSprite;
        SRenderGeometry Geometry;
        SMaterialPushConstants PushConstants;
        SDrawCommand Draw;
    };

    /**
     * @brief Receives material-backed draws for the current frame.
     */
    class ELIXIR_API MaterialSubmissionSink
    {
    public:
        virtual ~MaterialSubmissionSink() = default;

        /**
         * @brief Adds a draw to the current material frame.
         */
        virtual void Submit(SMaterialDrawSubmission Submission) = 0;
    };
}

namespace Elixir::Materials
{
    /**
     * @brief Coordinates material rendering for the application.
     *
     * The system collects material-backed draws, prepares shared material
     * resources, builds render batches, and records their draw commands.
     */
    class ELIXIR_API MaterialSystem final
        : public Rendering::MaterialSubmissionSink
    {
    public:
        /**
         * @brief Starts material submission for a frame.
         */
        void BeginFrame(const Rendering::SMaterialFrameContext& Context);

        /**
         * @brief Adds a material-backed draw to the current frame.
         */
        void Submit(Rendering::SMaterialDrawSubmission Submission) override;

        /**
         * @brief Prepares and renders all draws submitted for the frame.
         */
        SMaterialRenderResult RenderFrame();

    private:
        struct SFrameState;

        void PrepareFrame(SFrameState& Frame);
        void BuildBatches(SFrameState& Frame);
        SMaterialRenderResult ExecuteBatches(SFrameState& Frame);
    };
}
```

`MaterialSubmissionSink` evita que produtores dependam da API completa de
`MaterialSystem`. Caso a interface não traga benefício concreto durante a
implementação, o próprio `MaterialSystem` pode ser usado como sink sem mudar o
fluxo nem as responsabilidades.

## Estado interno do frame

O estado preparado deve ser uma implementação privada de `MaterialSystem`:

```cpp
struct MaterialSystem::SFrameState
{
    Rendering::SMaterialFrameContext Context;
    Rendering::MaterialRenderScene Scene;
    Ref<const Rendering::FrameTable> MaterialTable;
    std::vector<SMaterialBatch> Batches;
};
```

O conteúdo exato pode variar, mas deve incluir:

- identificação do frame ou submission;
- submissões ou uma `MaterialRenderScene` agregada;
- tabela deduplicada de materiais;
- batches preparados para execução;
- recursos cuja vida útil precisa cobrir o uso pela GPU.

Deve existir um slot por frame em voo, selecionado pelo frame index ou pelo
submission serial fornecido pelo `GraphicsContext`. Um único `SPreparedFrame`
mutável não é suficiente quando a CPU pode começar outro frame antes de a GPU
terminar o anterior.

O ciclo interno esperado é:

```text
Idle --> Collecting --> Preparing --> Rendering --> Submitted
           ^                                      |
           +----------- reusable frame slot <-----+
```

`Submit` só é válido durante `Collecting`. `RenderFrame` sela a coleção para que
nenhum produtor altere a cena enquanto batches e comandos estão sendo criados.

## Representação do material nas submissões

Preferencialmente, produtores devem submeter `MaterialInstance` ou um
`MaterialHandle` pertencente ao sistema, e não um `MaterialRenderProxy`.

Isso garante que:

- todo material seja resolvido pelo sistema central;
- proxies compilados continuem sendo detalhes de rendering;
- fallback e invalidação sejam uniformes;
- materiais de diferentes produtores sejam deduplicados conjuntamente;
- índices de materiais e texturas sejam atribuídos globalmente por frame.

Se a migração imediata de `MaterialRenderProxy` não for viável, ele pode ser
aceito temporariamente em `SRenderItem`. Essa deve ser uma etapa intermediária,
não o contrato final de submissão.

## Mudanças no Aether

`Aether::Rendering::Renderer` deixa de executar materiais. Ele passa a preparar
os recursos específicos de partículas e a publicar draws genéricos:

```cpp
void Aether::Rendering::Renderer::SubmitRenderItems(
    const RenderFrame& Frame,
    const Camera& Camera,
    Materials::Rendering::MaterialSubmissionSink& Sink
);
```

Sua implementação deve:

1. atualizar os dados de câmera e do frame;
2. montar geometria, buffers externos, push constants e draw ranges;
3. enviar cada draw ao sink;
4. atualizar métricas de submissão que pertencem ao Aether.

`BeginRendering`, `EndRendering`, `MaterialSystem::PrepareFrame` e
`MaterialSystem::Render` deixam de fazer parte desse fluxo.

A simulação continua no Aether. Os comandos de compute devem ser enfileirados
antes dos comandos gráficos do `MaterialSystem`, ou a dependência deve ser
representada explicitamente quando existir um render graph.

`Aether::Manager` não precisa manter uma referência permanente ao
`MaterialSystem` apenas para rendering. A dependência pode ser passada como
`MaterialSubmissionSink&` no momento da submissão. Se `Aether::Runtime` ainda
precisar resolver materiais durante compilação de assets, essa dependência deve
ser analisada separadamente da execução gráfica.

## Mudanças para meshes

O renderer de meshes deve usar o mesmo sink e o mesmo formato de submissão. A
primeira implementação pode tratar todos os draws como dinâmicos:

```cpp
MeshRenderer.SubmitRenderItems(Scene, Camera, MaterialSystem);
```

Depois que o fluxo compartilhado estiver estável, meshes estáticas podem ganhar
um caminho persistente:

```cpp
MaterialPrimitiveHandle RegisterPrimitive(
    const SMaterialPrimitiveDescription& Primitive
);

void UpdatePrimitive(
    MaterialPrimitiveHandle Handle,
    const SMaterialPrimitiveUpdate& Update
);

void RemovePrimitive(MaterialPrimitiveHandle Handle);
```

Partículas continuam usando submissões dinâmicas por frame. Meshes que mudam
pouco podem reutilizar geometria, classificação por pass e parte dos comandos.

## Relação com a arquitetura da Unreal Engine

O desenho segue a mesma separação conceitual da mesh drawing pipeline da Unreal:

| Elixir | Equivalente conceitual na Unreal |
| --- | --- |
| Aether ou mesh renderer | `FPrimitiveSceneProxy` |
| `SMaterialDrawSubmission` | `FMeshBatch` |
| `MaterialRenderProxy` | `FMaterialRenderProxy` |
| preparação específica do pass | `FMeshPassProcessor` |
| batch executável | `FMeshDrawCommand` |
| `GraphicsContext` e command buffer | RHI e `RHICommandList` |

A correspondência não é literal. Na Unreal, o scene renderer é o dono da
execução e materiais são entradas dos draws. No Elixir, `MaterialSystem` pode ser
a fachada pública única exigida pela aplicação, desde que internamente preserve
a separação entre:

- coleta e estado global do frame;
- resolução e recursos de materiais;
- preparação de passes e pipelines;
- emissão de comandos gráficos.

O `Materials::Rendering::Renderer` existente ocupa a camada interna de
preparação do material e do pipeline. Não deve ser duplicado.

## API que deve deixar de ser pública

Ao final da migração, estas operações devem ser privadas ou removidas da API
pública de `MaterialSystem`:

- `PrepareFrame(const MaterialRenderScene&, uint64_t)`;
- `Render(CommandBuffer, MaterialRenderScene, uint64_t)`;
- `GetProgramKey`;
- `PrepareMaterialPass`;
- acesso direto ao frame buffer de materiais;
- acesso direto ao texture set e sampler quando usados apenas pelo renderer
  interno.

`SPreparedFrame` e qualquer futuro snapshot preparado devem permanecer privados.
`SMaterialRenderResult` pode continuar público porque contém somente métricas do
trabalho realizado e não é necessário para completar a execução do frame.

## Etapas de implementação

### 1. Introduzir a coleta central

- Adicionar o contexto público do frame.
- Adicionar `BeginFrame`, `Submit` e `RenderFrame`.
- Fazer `MaterialSystem` possuir a `MaterialRenderScene` agregada.
- Manter temporariamente os métodos antigos enquanto consumidores são migrados.
- Adicionar asserts para transições inválidas do ciclo do frame.

### 2. Internalizar preparação e execução

- Mover a lógica atual de `PrepareFrame` para um método privado que opere sobre
  o frame corrente.
- Fazer `RenderFrame` construir a tabela, os batches e executar os draws em uma
  única operação pública.
- Reutilizar `Materials::Rendering::Renderer` para resolução, programa, bindings
  e pipeline.
- Remover a necessidade de passar `submissionSerial` de volta à execução.

### 3. Migrar o Aether

- Substituir `BuildMaterialRenderScene` seguido de preparação e renderização por
  submissões ao sink.
- Remover chamadas de ciclo de vida do `MaterialSystem` do renderer do Aether.
- Mover o início e o fim do escopo gráfico para o coordenador central.
- Preservar a execução e a ordenação dos comandos de simulação.
- Ajustar métricas para diferenciar itens submetidos de draws efetivamente
  executados.

### 4. Centralizar o ciclo na Application

- Iniciar o frame do `MaterialSystem` dentro do callback de renderização.
- Permitir que o `Application::Render` e seus subsistemas apenas submetam draws.
- Chamar `MaterialSystem::RenderFrame` depois que todos os produtores terminarem.
- Renderizar GUI na ordem definida pela aplicação.
- Garantir que nenhuma chamada de produtor possa encerrar o frame central.

### 5. Adicionar meshes

- Traduzir meshes para o mesmo contrato de submissão.
- Confirmar que meshes e partículas aparecem na mesma `FrameTable`.
- Validar ordenação de passes e compartilhamento de materiais entre produtores.
- Avaliar posteriormente um caminho retido para primitivas estáticas.

### 6. Remover a API antiga

- Remover `PrepareFrame` e `Render` públicos antigos.
- Remover getters usados apenas para vazar recursos internos.
- Remover dependências permanentes desnecessárias do Aether em
  `MaterialSystem`.
- Atualizar documentação e exemplos.

### 7. Suportar múltiplos frames em voo

- Substituir o único `SPreparedFrame` por slots de frame.
- Vincular cada slot ao mecanismo de frame index ou submission serial do
  `GraphicsContext`.
- Reutilizar um slot somente depois que os recursos correspondentes puderem ser
  atualizados com segurança.
- Garantir que `FrameTable`, buffers e registros de textura tenham vida útil
  suficiente.

## Pontos que precisam de decisão durante a implementação

### Ownership do command buffer

A opção preferida é `MaterialSystem::RenderFrame` criar e enfileirar seu command
buffer secundário usando o `GraphicsContext` que já recebe no construtor. Isso
torna o sistema realmente responsável pela execução dos materiais.

Se a aplicação precisar compor vários sistemas no mesmo command buffer, o
contexto do frame poderá fornecer um command buffer, mas produtores ainda não
devem fornecê-lo diretamente ao renderer interno.

### Render targets e múltiplas views

A primeira versão pode aceitar um color target, um depth target e uma render
area no contexto do frame. Antes de suportar múltiplas câmeras, sombras ou
render-to-texture, será necessário introduzir uma identificação de view ou
render scope nas submissões.

### MaterialInstance ou MaterialHandle

`MaterialInstance` simplifica a primeira migração. Um `MaterialHandle` estável
pode ser introduzido depois para remover ownership compartilhado dos draws e
facilitar cache e invalidação.

### Interface de submissão

`MaterialSubmissionSink` reduz o acoplamento dos produtores. Ela não deve crescer
para expor preparação, resolução ou recursos internos. Se a única implementação
for `MaterialSystem` e a interface não ajudar testes ou dependências, ela pode
ser removida sem mudar o modelo arquitetural.

## Invariantes

- Existe somente um `MaterialSystem` por `Application`.
- Todos os draws que usam materiais entram pelo frame corrente do sistema.
- Apenas `Application` controla o início e a execução do frame de materiais.
- Aether e meshes nunca chamam preparação ou execução de materiais.
- O estado preparado nunca sai do `MaterialSystem`.
- A tabela de materiais representa conjuntamente todos os produtores do frame.
- O renderer interno não conhece os produtores.
- Um frame em voo não sobrescreve recursos ainda utilizados por outro frame.

## Validação

### Comportamento

- Um frame contendo somente partículas mantém o resultado visual atual.
- Um frame contendo somente meshes renderiza pela mesma pipeline central.
- Partículas e meshes podem coexistir no mesmo frame.
- Uma mesma instância de material usada por ambos ocupa uma única entrada na
  tabela do frame.
- Um frame sem submissões termina sem comandos de draw.
- Submissões fora de `BeginFrame` e depois do início de `RenderFrame` são
  rejeitadas por assert ou por resultado explícito.
- Falhas de resolução usam o fallback definido pelo sistema sem interromper os
  demais draws.

### Ordem e sincronização

- Compute do Aether termina ou é corretamente sincronizado antes dos draws que
  consomem seus buffers.
- Passes respeitam a ordem definida pelo renderer de materiais.
- GUI é executada na ordem prevista em relação aos materiais.
- Slots de frames em voo não compartilham estado mutável de forma insegura.

### Métricas

- `MaterialCount` representa materiais únicos do frame inteiro.
- `BatchCount` representa batches executados pelo sistema central.
- `DrawCount` representa draws efetivamente gravados.
- Métricas específicas do Aether continuam disponíveis sem assumir que ele é o
  único produtor.

### Compatibilidade de plataformas

- Validar compilação e comportamento em macOS e Windows.
- Verificar símbolos que cruzam a fronteira da DLL e aplicar `ELIXIR_API` aos
  tipos e funções públicos necessários.
- Confirmar que novas APIs não dependem de símbolos resolvidos apenas por
  linking estático no macOS.
- Executar testes de unidade e integração em Windows no CI.
- Manter validações específicas de Vulkan e de lifetime de command buffers em
  ambas as plataformas.

## Critérios de conclusão

O trabalho estará concluído quando:

1. `Application` controlar o ciclo de materiais uma única vez por frame;
2. Aether apenas submeter draws e não chamar preparação ou execução do sistema;
3. meshes usarem o mesmo caminho de submissão;
4. `MaterialSystem` construir uma tabela conjunta para todos os produtores;
5. o `Materials::Rendering::Renderer` existente continuar como único executor
   interno de materiais;
6. nenhum snapshot preparado fizer parte do contrato público;
7. múltiplos frames em voo forem tratados sem sobrescrita prematura;
8. testes relevantes passarem em macOS e Windows.
