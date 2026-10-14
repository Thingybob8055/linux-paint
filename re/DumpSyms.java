import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import java.io.*;
public class DumpSyms extends GhidraScript {
  public void run() throws Exception {
    PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[0]));
    SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
    while (it.hasNext()) { Symbol s = it.next(); if (s.isExternal()) continue;
      w.println(s.getAddress()+"\t"+s.getSymbolType()+"\t"+s.getName(true)); }
    w.close();
  }
}
