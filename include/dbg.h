/* dbg - the model's built-in pixel watchpoint.
 *
 * Three free functions, no class in the interface: an application sets a
 * (x,y) watch position and a GPU2 to look into with DbgInit(), arms it
 * with DbgMode(), and every pixel write calls DbgWatch().  When the write
 * hits the watched pixel the current Pre3 vertex queue and the PCalc
 * z/rgba slopes are dumped to stdout.
 *
 * Only DbgWatch() is called from inside the library (memory.o, from
 * FBConfig::WritePixel and friends); DbgInit/DbgMode are for the host
 * program.  The Dbg class itself and the pipeline views it needs are
 * private to src/dbg.c - see doc/notes/dbg.md.
 */

void DbgInit(int mode, void *p);
void DbgMode(int mode);
void DbgWatch(int type, int x, int y);
