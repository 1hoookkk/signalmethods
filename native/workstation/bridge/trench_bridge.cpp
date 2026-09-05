#include "bridge_values.hpp"

void mexFunction(int nl,mxArray** lhs,int nr,const mxArray** rhs) {
    std::string command="command";
    try {
        mb::require(nr>0,"Command required"); command=mb::str(rhs[0]); std::vector<mxArray*> out;
        mb::require(mb::chordCommand(command,nr,rhs,out) || mb::dataCommand(command,nr,rhs,out),"Unknown command");
        mb::require(nl<=int(out.size()),"Too many outputs");
        for(int i=0;i<int(out.size());++i) { if(i<nl) lhs[i]=out[i]; else mxDestroyArray(out[i]); }
    } catch(const std::exception& e) { mexErrMsgIdAndTxt(("trench:bridge:"+command).c_str(),"%s",e.what()); }
}
