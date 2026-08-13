#include <map>
#include <vector>
#include <set>
#include <ostream>
#include <algorithm>

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class rpo {
private:
    enum class color {
        White,  // непосещенная верщина
        Gray,   // вершина в обработке
        Black   // посещена
};
    Function& func_;
    std::unordered_map<size_t, color> color_map_;
    std::vector<std::string> postorder_;
    std::vector<std::string> loops_;
    const std::map<BasicBlock*, size_t> &BBMap_;
public:
    explicit rpo(Function &func, std::map<BasicBlock*, size_t>& bbMap) : func_(func), BBMap_(bbMap) {}
    void dfs(BasicBlock *current) {
        size_t block_id = BBMap_.at(current);
        color_map_[block_id] = color::Gray;      // при посещении вершины перекрашиваем в Gray
        const Instruction *terminator = current->getTerminator();
        size_t n = terminator->getNumSuccessors();
        for (size_t i = 0; i < n; ++i) {
            BasicBlock *succ = terminator->getSuccessor(i);
            size_t succ_id = BBMap_.at(succ);
            auto color_iter = color_map_.find(succ_id);
            color neigh_color = color_iter == color_map_.end() ? color::White : color_iter->second;
            if (neigh_color == color::White) {  // если вершина не посещена - запускаем из нее dfs
                dfs(succ);
            }
            else if (neigh_color == color::Gray) {  // если мы дошли до серой вершины - значит обнаружили цикл
                loops_.push_back("Found loop " + std::to_string(block_id) + "->" + std::to_string(succ_id));
            }
        }
        color_map_[block_id] = color::Black;     // красим посещенную вершину в черный
        postorder_.push_back(std::to_string(block_id));          // добавляем в порядок обхода
    }
    std::vector<std::string>& get_postorder() { return postorder_; }
    std::vector<std::string>& get_loops() { return loops_; }
};

void
VisitFunction( Function &Func)
{
    /* LLVM использует кастомные потоки вывода. 
     * llvm::errs() (поток ошибок), 
     * llvm::outs() (аналогичен стандартному stdout), 
     * llvm::nulls() (отбрасывает весь вывод) */
    if (Func.empty()){
        return;
    }
    outs() << "Visiting function: " << Func.getName() << "\n";

    std::map<BasicBlock*, size_t> BBMap;
    size_t counter = 0;
    BasicBlock *Entry = &Func.getEntryBlock();
    for (auto &BB : Func){
        BBMap.emplace(&BB, counter++);
    }
    auto rpo_class = rpo(Func, BBMap);
    rpo_class.dfs(Entry);
    auto& post_order = rpo_class.get_postorder();
    std::reverse(post_order.begin(), post_order.end());
    auto& loops = rpo_class.get_loops();
    std::string res;
    for (auto& loop : loops) {
        res += loop + "\n";
    }
    for (auto& vertex : post_order) {
        res += vertex + " ";
    }
    if (!res.empty()) {
        res.pop_back();
    }
    outs() << res << "\n\n";
}


struct RpoPass : PassInfoMixin<RpoPass>
{
    /* PreservedAnalyses - множество анализов, которые сохраняются после данного прохода,
     * чтобы не запускать их заново.
     *
     * run() непосредственно нужен для запуска прохода.
     *
     * Так как мы просто хотим вывести: "имя функции - количество аргументов", то мы 
     * возвращаем all(), что говорит о том, что ни один анализ не будет испорчен */
    PreservedAnalyses 
    run(Function &Function,
        FunctionAnalysisManager &AnalysisManager)
    {
        VisitFunction( Function);
        return (PreservedAnalyses::all());
    }

    /* По умолчанию данный проход будет пропущен, если функция помечена атрибутом 
     * optnone (не производить оптимизаций над ней). Поэтому необходимо вернуть true, 
     * чтобы мы могли обходить и их. 
     * (в режиме сборки -O0 все функции помечены как неоптимизируемые) */
    static bool 
    isRequired( void) 
    { 
        return (true); 
    }
};
} /* namespace */

/**
 * Наш проход будет реализован в виде отдельно подключаемого плагина (расширения языка).
 * Это удобный способ расширить возможности компилятора. Например, сделать поддержку
 * своей прагмы, своей оптимизации, выдачи своего предупреждения.
 *
 * PassPluginLibraryInfo - структура, задающая базовые параметры для нашего плагина.
 * Её надо составить из:
 * - Версии API (для отслеживания совместимости ABI можно 
 *   использовать LLVM_PLUGIN_API_VERSION)
 * - Имя плагина
 * - Версии плагина
 * - Callback для регистрации плагина через PassBuilder
 */
/**
 * По-модному это делают с помощью лямбда-функции, но можно и по-старинке.
 */
bool
CallBackForPipelineParser(
    StringRef Name,
    FunctionPassManager &FPM,  
    ArrayRef<PassBuilder::PipelineElement>)
{
    if ( Name == "RpoPass" )
    {
        FPM.addPass( RpoPass());
	return (true);
    } else
    {
        return (false);
    }
} /* CallBackForPipelineParser */

void
CallBackForPassBuilder( PassBuilder &PB)
{
    PB.registerPipelineParsingCallback( &CallBackForPipelineParser);
} /* CallBackForPassBuider */

PassPluginLibraryInfo 
getRpoPassPluginInfo( void)
{
    uint32_t     APIversion =  LLVM_PLUGIN_API_VERSION;
    const char * PluginName =  "RpoPass";
    const char * PluginVersion =  LLVM_VERSION_STRING;
    
    PassPluginLibraryInfo info = 
    {
        APIversion,
	    PluginName, 
	    PluginVersion, 
	    CallBackForPassBuilder
    };

    return (info);
} /* getMyPassPluginInfo */

/**
 * Интерфейс, который гарантирует, что "opt" распознаст наш проход. 
 * "-passes=MyPass"
 */
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() 
{
    return (getRpoPassPluginInfo());
} /* llvmGetPassPluginInfo */
