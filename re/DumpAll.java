import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import java.io.*;
public class DumpAll extends GhidraScript {
  public void run() throws Exception {
    String out = getScriptArgs()[0];
    PrintWriter fl = new PrintWriter(new FileWriter(out + "/functions.tsv"));
    PrintWriter dc = new PrintWriter(new FileWriter(out + "/decomp.c"));
    PrintWriter st = new PrintWriter(new FileWriter(out + "/strings.tsv"));
    DataIterator di = currentProgram.getListing().getDefinedData(true);
    while (di.hasNext()) { Data d = di.next(); if (d.hasStringValue()) { Object v=d.getValue(); st.println(d.getAddress()+"\t"+String.valueOf(v).replace("\n","\\n").replace("\t","\\t")); } }
    st.close();
    DecompInterface ifc = new DecompInterface(); ifc.openProgram(currentProgram);
    FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
    while (it.hasNext() && !monitor.isCancelled()) {
      Function f = it.next();
      fl.println(f.getEntryPoint()+"\t"+f.getBody().getNumAddresses()+"\t"+f.getName(true));
      if (f.isThunk() || f.isExternal()) continue;
      String n=f.getName(true);
      if (n.startsWith("std::")||n.startsWith("Microsoft::WRL")||n.startsWith("wil::")||n.startsWith("__")||n.startsWith("_")) continue;
      DecompileResults r = ifc.decompileFunction(f, 60, monitor);
      dc.println("// ==== " + n + " @ " + f.getEntryPoint());
      if (r != null && r.decompileCompleted()) dc.println(r.getDecompiledFunction().getC()); else dc.println("// decompile failed");
    }
    fl.close(); dc.close();
  }
}
