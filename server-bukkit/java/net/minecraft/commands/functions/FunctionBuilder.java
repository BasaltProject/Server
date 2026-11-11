// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.functions;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.UnboundEntryAction;
import net.minecraft.resources.Identifier;
class FunctionBuilder<T extends ExecutionCommandSource<T>> {
    private List<UnboundEntryAction<T>> plain = new ArrayList<>();
    private List<MacroFunction.Entry<T>> macro;
    private final List<String> parameters = new ArrayList<>();
    public void addCommand(UnboundEntryAction<T> command) {
        if (macro != null) {
            macro.add(new MacroFunction.PlainTextEntry<>(command));
        } else {
            plain.add(command);
        }
    }
    private int indexOf(String id) {
        int index = parameters.indexOf(id);
        if (index == -1) {
            index = parameters.size();
            parameters.add(id);
        }
        return index;
    }
    private IntList indicesOf(List<String> ids) {
        IntArrayList out = new IntArrayList(ids.size());
        ids.forEach(id -> out.add(indexOf(id)));
        return out;
    }
    public void addMacro(String command, int line, T compilationContext) {
        StringTemplate template;
        try {
            template = StringTemplate.fromString(command);
        } catch (Exception e) {
            throw new IllegalArgumentException("Can't parse function line " + line + ": '" + command + "'", e);
        }
        if (plain != null) {
            macro = new ArrayList<>(plain.size() + 1);
            plain.forEach(action -> macro.add(new MacroFunction.PlainTextEntry<>(action)));
            plain = null;
        }
        macro.add(new MacroFunction.MacroEntry<>(template, indicesOf(template.variables()), compilationContext));
    }
    public CommandFunction<T> build(Identifier id) {
        return macro != null ? new MacroFunction<>(id, macro, parameters) : new PlainTextFunction<>(id, plain);
    }
}
