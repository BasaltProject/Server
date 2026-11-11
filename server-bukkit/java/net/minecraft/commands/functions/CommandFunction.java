// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.functions;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.List;
import net.minecraft.commands.Commands;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.FunctionInstantiationException;
import net.minecraft.commands.execution.UnboundEntryAction;
import net.minecraft.commands.execution.tasks.BuildContexts;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
public interface CommandFunction<T> {
    int kMaxLineLength = 2000000;
    Identifier id();
    InstantiatedFunction<T> instantiate(CompoundTag arguments, CommandDispatcher<T> dispatcher) throws FunctionInstantiationException;
    private static boolean continuesOnNextLine(CharSequence line) {
        return line.length() > 0 && line.charAt(line.length() - 1) == '\\';
    }
    static <T extends ExecutionCommandSource<T>> CommandFunction<T> fromLines(Identifier id, CommandDispatcher<T> dispatcher, T compilationContext, List<String> lines) {
        FunctionBuilder<T> builder = new FunctionBuilder<>();
        for (int i = 0; i < lines.size(); i++) {
            int number = i + 1;
            String trimmed = lines.get(i).trim();
            String line = trimmed;
            if (continuesOnNextLine(trimmed)) {
                StringBuilder joined = new StringBuilder(trimmed);
                do {
                    if (++i == lines.size()) {
                        throw new IllegalArgumentException("Line continuation at end of file");
                    }
                    joined.deleteCharAt(joined.length() - 1);
                    joined.append(lines.get(i).trim());
                    checkCommandLineLength(joined);
                } while (continuesOnNextLine(joined));
                line = joined.toString();
            }
            checkCommandLineLength(line);
            StringReader reader = new StringReader(line);
            if (!reader.canRead() || reader.peek() == '#') {
                continue;
            }
            if (reader.peek() == '/') {
                reader.skip();
                if (reader.peek() == '/') {
                    throw new IllegalArgumentException("Unknown or invalid command '" + line + "' on line " + number + " (if you intended to make a comment, use '#' not '//')");
                }
                throw new IllegalArgumentException("Unknown or invalid command '" + line + "' on line " + number + " (did you mean '" + reader.readUnquotedString() + "'? Do not use a preceding forwards slash.)");
            }
            if (reader.peek() == '$') {
                builder.addMacro(line.substring(1), number, compilationContext);
                continue;
            }
            try {
                builder.addCommand(parseCommand(dispatcher, compilationContext, reader));
            } catch (CommandSyntaxException e) {
                throw new IllegalArgumentException("Whilst parsing command on line " + number + ": " + e.getMessage());
            }
        }
        return builder.build(id);
    }
    static void checkCommandLineLength(CharSequence line) {
        if (line.length() > kMaxLineLength) {
            throw new IllegalStateException("Command too long: " + line.length() + " characters, contents: " + line.subSequence(0, 512) + "...");
        }
    }
    static <T extends ExecutionCommandSource<T>> UnboundEntryAction<T> parseCommand(CommandDispatcher<T> dispatcher, T compilationContext, StringReader input) throws CommandSyntaxException {
        ParseResults<T> parsed = dispatcher.parse(input, compilationContext);
        Commands.validateParseResults(parsed);
        var chain = ContextChain.tryFlatten(parsed.getContext().build(input.getString()));
        if (chain.isEmpty()) {
            throw CommandSyntaxException.BUILT_IN_EXCEPTIONS.dispatcherUnknownCommand().createWithContext(parsed.getReader());
        }
        return new BuildContexts.Unbound<>(input.getString(), chain.get());
    }
}
