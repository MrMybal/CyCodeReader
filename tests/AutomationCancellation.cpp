#include "Automation.h"
#include <atomic>
#include <iostream>
#include <thread>
using namespace cycode::reader;
int main(int argc,char **argv){
    if(argc!=2)return 2;
    const auto base=std::filesystem::absolute(argv[1]);
    const auto data=base/("cancel-"+cymcp::random_token(16));
    try{
        {
            Core core;std::atomic_bool entered=false;
            Automation host(data,[&](const auto &m,const auto &p,const auto &c){entered=true;return dispatchTextSession(core,m,p,c);},false);
            const auto original=host.invoke("documents_new",Json::object(),"edit");entered=false;
            // Keep future before guard: exception unwinding must unlock before joining.
            std::future<Json> pending;
            std::unique_lock guard(core.mutex());
            pending=std::async(std::launch::async,[&]{return host.invoke("documents_replace",{{"id",original.at("id")},{"expectedRevision",original.at("revision")},{"text","must not commit"}},"edit");});
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(!entered&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if(!entered)throw std::runtime_error("Dispatcher did not start");
            if(pending.wait_for(std::chrono::milliseconds(50))!=std::future_status::timeout)throw std::runtime_error("Native call did not wait for the document lock");
            std::string id;
            const auto jobs=host.invoke("jobs_list",Json::object(),"edit");
            for(const auto &job:jobs.at("jobs"))if(job.at("operation")=="documents_replace"&&job.at("status")=="running")id=job.at("id");
            if(id.empty())throw std::runtime_error("Active durable receipt missing");
            if(!host.invoke("jobs_cancel",{{"id",id},{"cancel",true}},"edit").at("cancellationRequested"))throw std::runtime_error("Active cancellation was not requested");
            guard.unlock();bool cancelled=false;
            try{pending.get();}catch(const cymcp::Error &e){cancelled=e.code=="Cancelled";}
            if(!cancelled)throw std::runtime_error("A cancelled queued mutation executed");
            const auto result=core.dispatch("documents.read",{{"id",original.at("id")}});
            if(result.at("text")!=""||result.at("revision")!=original.at("revision"))throw std::runtime_error("Queued cancellation changed the document");
            if(host.invoke("jobs_get",{{"id",id}},"edit").at("status")!="cancelled")throw std::runtime_error("Cancelled receipt was not durable");
        }
        // Delete only this test's issued child, after releasing the profile lease.
        if(std::filesystem::weakly_canonical(data).parent_path()!=std::filesystem::weakly_canonical(base)||!data.filename().string().starts_with("cancel-"))throw std::runtime_error("Unexpected test cleanup path");
        std::filesystem::remove_all(data);
        std::cout<<"Queued native mutation cancelled after lock acquisition; text/revision unchanged and receipt persisted.\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
