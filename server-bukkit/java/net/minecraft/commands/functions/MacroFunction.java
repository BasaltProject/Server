// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.functions;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import it.unimi.dsi.fastutil.ints.IntList;
import it.unimi.dsi.fastutil.ints.IntLists;
import it.unimi.dsi.fastutil.objects.Object2ObjectLinkedOpenHashMap;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.FunctionInstantiationException;
import net.minecraft.commands.execution.UnboundEntryAction;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
public class MacroFunction<T extends ExecutionCommandSource<T>> implements CommandFunction<T> {
    private static final int kCacheEntries = 8;
    private static final DecimalFormat kDecimal = new DecimalFormat("#", DecimalFormatSymbols.getInstance(Locale.ROOT));
    static {
        kDecimal.setMaximumFractionDigits(15);
    }
    private final List<String> parameters;
    private final Object2ObjectLinkedOpenHashMap<List<String>, InstantiatedFunction<T>> cache = new Object2ObjectLinkedOpenHashMap<>(kCacheEntries, 0.25F);
    private final Identifier id;
    private final List<Entry<T>> entries;
    public MacroFunction(Identifier id, List<Entry<T>> entries, List<String> parameters) {
        this.id = id;
        this.entries = entries;
        this.parameters = parameters;
    }
    public Identifier id() {
        return id;
    }
    public InstantiatedFunction<T> instantiate(CompoundTag arguments, CommandDispatcher<T> dispatcher) throws FunctionInstantiationException {
        if (arguments == null) {
            throw new FunctionInstantiationException(Component.translatable("commands.function.error.missing_arguments", Component.translationArg(id)));
        }
        List<String> values = new ArrayList<>(parameters.size());
        for (String name : parameters) {
            Tag value = arguments.get(name);
            if (value == null) {
                throw new FunctionInstantiationException(Component.translatable("commands.function.error.missing_argument", Component.translationArg(id), name));
            }
            values.add(stringify(value));
        }
        InstantiatedFunction<T> hit = cache.getAndMoveToLast(values);
        if (hit != null) {
            return hit;
        }
        if (cache.size() >= kCacheEntries) {
            cache.removeFirst();
        }
        InstantiatedFunction<T> built = build(parameters, values, dispatcher);
        cache.put(values, built);
        return built;
    }
    private static String stringify(Tag tag) {
        if (tag instanceof FloatTag f) {
            return kDecimal.format((double) f.value());
        }
        if (tag instanceof DoubleTag d) {
            return kDecimal.format(d.value());
        }
        if (tag instanceof ByteTag b) {
            return String.valueOf(b.value());
        }
        if (tag instanceof ShortTag s) {
            return String.valueOf(s.value());
        }
        if (tag instanceof LongTag l) {
            return String.valueOf(l.value());
        }
        if (tag instanceof StringTag s) {
            return s.value();
        }
        return tag.toString();
    }
    private InstantiatedFunction<T> build(List<String> keys, List<String> values, CommandDispatcher<T> dispatcher) throws FunctionInstantiationException {
        List<UnboundEntryAction<T>> actions = new ArrayList<>(entries.size());
        List<String> picked = new ArrayList<>(values.size());
        for (Entry<T> entry : entries) {
            picked.clear();
            entry.parameters().forEach(index -> picked.add(values.get(index)));
            actions.add(entry.instantiate(picked, dispatcher, id));
        }
        return new PlainTextFunction<>(id.withPath(path -> path + "/" + keys.hashCode()), actions);
    }
    static class PlainTextEntry<T> implements Entry<T> {
        private final UnboundEntryAction<T> action;
        public PlainTextEntry(UnboundEntryAction<T> compiledAction) {
            this.action = compiledAction;
        }
        public IntList parameters() {
            return IntLists.emptyList();
        }
        public UnboundEntryAction<T> instantiate(List<String> substitutions, CommandDispatcher<T> dispatcher, Identifier functionId) {
            return action;
        }
    }
    static class MacroEntry<T extends ExecutionCommandSource<T>> implements Entry<T> {
        private final StringTemplate template;
        private final IntList indices;
        private final T context;
        public MacroEntry(StringTemplate template, IntList parameters, T compilationContext) {
            this.template = template;
            this.indices = parameters;
            this.context = compilationContext;
        }
        public IntList parameters() {
            return indices;
        }
        public UnboundEntryAction<T> instantiate(List<String> substitutions, CommandDispatcher<T> dispatcher, Identifier functionId) throws FunctionInstantiationException {
            String command = template.substitute(substitutions);
            try {
                return CommandFunction.parseCommand(dispatcher, context, new StringReader(command));
            } catch (CommandSyntaxException e) {
                throw new FunctionInstantiationException(Component.translatable("commands.function.error.parse", Component.translationArg(functionId), command, e.getMessage()));
            }
        }
    }
    interface Entry<T> {
        IntList parameters();
        UnboundEntryAction<T> instantiate(List<String> substitutions, CommandDispatcher<T> dispatcher, Identifier functionId) throws FunctionInstantiationException;
    }
}
