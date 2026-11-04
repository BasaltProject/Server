// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.google.common.collect.Lists;
import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntMaps;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Supplier;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.nbt.CollectionTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.chat.Component;

public class NbtPathArgument implements ArgumentType<NbtPathArgument.NbtPath> {
    private static final Collection<String> EXAMPLES = Arrays.asList("foo", "foo.bar", "foo[0]", "[0]", "[]", "{foo=bar}");
    public static final SimpleCommandExceptionType ERROR_INVALID_NODE = new SimpleCommandExceptionType(Component.translatable("arguments.nbtpath.node.invalid"));
    public static final SimpleCommandExceptionType ERROR_DATA_TOO_DEEP = new SimpleCommandExceptionType(Component.translatable("arguments.nbtpath.too_deep"));
    public static final DynamicCommandExceptionType ERROR_NOTHING_FOUND = new DynamicCommandExceptionType(path -> Component.translatableEscape("arguments.nbtpath.nothing_found", path));
    private static final char kIndexMatchStart = '[';
    private static final char kIndexMatchEnd = ']';
    private static final char kSelectorStart = '{';
    private static final char kSelectorEnd = '}';
    private static final char kDoubleQuote = '"';
    private static final char kSingleQuote = '\'';
    private static final int kMaxDepth = 512;

    public static NbtPathArgument nbtPath() {
        return new NbtPathArgument();
    }

    public static NbtPath getPath(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, NbtPath.class);
    }

    public NbtPath parse(StringReader reader) throws CommandSyntaxException {
        List<Node> nodes = Lists.newArrayList();
        int start = reader.getCursor();
        Object2IntMap<Node> positions = new Object2IntOpenHashMap<>();
        boolean first = true;
        while (reader.canRead() && reader.peek() != ' ') {
            Node node = parseNode(reader, first);
            nodes.add(node);
            positions.put(node, reader.getCursor() - start);
            first = false;
            if (reader.canRead()) {
                char next = reader.peek();
                if (next != ' ' && next != kIndexMatchStart && next != kSelectorStart) {
                    reader.expect('.');
                }
            }
        }
        return new NbtPath(reader.getString().substring(start, reader.getCursor()), nodes.toArray(new Node[0]), positions);
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }

    private static Node parseNode(StringReader reader, boolean first) throws CommandSyntaxException {
        switch (reader.peek()) {
            case kSelectorStart:
                if (!first) {
                    throw ERROR_INVALID_NODE.createWithContext(reader);
                }
                return new MatchRootObjectNode(TagParser.parseCompoundAsArgument(reader));
            case kIndexMatchStart:
                reader.skip();
                char next = reader.peek();
                if (next == kSelectorStart) {
                    CompoundTag filter = TagParser.parseCompoundAsArgument(reader);
                    reader.expect(kIndexMatchEnd);
                    return new MatchElementNode(filter);
                }
                if (next == kIndexMatchEnd) {
                    reader.skip();
                    return AllElementsNode.INSTANCE;
                }
                int index = reader.readInt();
                reader.expect(kIndexMatchEnd);
                return new IndexedElementNode(index);
            case kDoubleQuote:
            case kSingleQuote:
                return readObjectNode(reader, reader.readString());
            default:
                return readObjectNode(reader, readUnquotedName(reader));
        }
    }

    private static Node readObjectNode(StringReader reader, String name) throws CommandSyntaxException {
        if (reader.canRead() && reader.peek() == kSelectorStart) {
            return new MatchObjectNode(name, TagParser.parseCompoundAsArgument(reader));
        }
        return new CompoundChildNode(name);
    }

    private static String readUnquotedName(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        while (reader.canRead() && isAllowedInUnquotedName(reader.peek())) {
            reader.skip();
        }
        if (reader.getCursor() == start) {
            throw ERROR_INVALID_NODE.createWithContext(reader);
        }
        return reader.getString().substring(start, reader.getCursor());
    }

    private static boolean isAllowedInUnquotedName(char c) {
        return c != ' ' && c != kDoubleQuote && c != kSingleQuote && c != kIndexMatchStart && c != kIndexMatchEnd && c != '.'
            && c != kSelectorStart && c != kSelectorEnd;
    }

    private static Predicate matches(CompoundTag pattern) {
        return tag -> NbtUtils.compareNbt(pattern, tag, true);
    }

    private interface Predicate {
        boolean test(Tag tag);
    }

    public static class NbtPath {
        public static final Codec<NbtPath> CODEC = Codec.STRING.comapFlatMap(text -> {
            try {
                return com.mojang.serialization.DataResult.success(of(text));
            } catch (CommandSyntaxException e) {
                return com.mojang.serialization.DataResult.error(() -> "Failed to parse path " + text + ": " + e.getMessage());
            }
        }, NbtPath::asString);
        private final String original;
        private final Object2IntMap<Node> nodeToOriginalPosition;
        private final Node[] nodes;

        public static NbtPath of(String string) throws CommandSyntaxException {
            return new NbtPathArgument().parse(new StringReader(string));
        }

        public NbtPath(String original, Node[] nodes, Object2IntMap<Node> nodeToOriginalPosition) {
            this.original = original;
            this.nodes = nodes;
            this.nodeToOriginalPosition = nodeToOriginalPosition;
        }

        public List<Tag> get(Tag tag) throws CommandSyntaxException {
            List<Tag> result = Collections.singletonList(tag);
            for (Node node : nodes) {
                result = node.get(result);
                if (result.isEmpty()) {
                    throw notFound(node);
                }
            }
            return result;
        }

        public int countMatching(Tag tag) {
            List<Tag> result = Collections.singletonList(tag);
            for (Node node : nodes) {
                result = node.get(result);
                if (result.isEmpty()) {
                    return 0;
                }
            }
            return result.size();
        }

        private List<Tag> getOrCreateParents(Tag tag) {
            List<Tag> result = Collections.singletonList(tag);
            for (int i = 0; i < nodes.length - 1; i++) {
                Node following = nodes[i + 1];
                result = nodes[i].getOrCreate(result, following::createPreferredParentTag);
                if (result.isEmpty()) {
                    return result;
                }
            }
            return result;
        }

        public List<Tag> getOrCreate(Tag tag, Supplier<Tag> newTagValue) {
            List<Tag> parents = getOrCreateParents(tag);
            Node last = nodes[nodes.length - 1];
            return last.getOrCreate(parents, newTagValue);
        }

        public static boolean isTooDeep(Tag tag, int depth) {
            if (depth >= kMaxDepth) {
                return true;
            }
            if (tag instanceof CompoundTag compound) {
                for (String key : compound.keySet()) {
                    Tag child = compound.get(key);
                    if (child != null && isTooDeep(child, depth + 1)) {
                        return true;
                    }
                }
            } else if (tag instanceof ListTag list) {
                for (Tag child : list) {
                    if (isTooDeep(child, depth + 1)) {
                        return true;
                    }
                }
            }
            return false;
        }

        public int set(Tag tag, Tag toAdd) throws CommandSyntaxException {
            if (isTooDeep(toAdd, nodes.length)) {
                throw ERROR_DATA_TOO_DEEP.create();
            }
            List<Tag> parents = getOrCreateParents(tag);
            if (parents.isEmpty()) {
                return 0;
            }
            Node last = nodes[nodes.length - 1];
            int changed = 0;
            for (Tag parent : parents) {
                changed += last.setTag(parent, toAdd::copy);
            }
            return changed;
        }

        public int insert(int index, CompoundTag target, List<Tag> toInsert) throws CommandSyntaxException {
            List<Tag> copies = new ArrayList<>(toInsert.size());
            for (Tag tag : toInsert) {
                Tag copy = tag.copy();
                copies.add(copy);
                if (isTooDeep(copy, nodes.length)) {
                    throw ERROR_DATA_TOO_DEEP.create();
                }
            }
            int changed = 0;
            for (Tag parent : getOrCreate(target, ListTag::new)) {
                if (!(parent instanceof CollectionTag collection)) {
                    throw notFound(nodes[nodes.length - 1]);
                }
                boolean changedHere = false;
                int at = index < 0 ? collection.size() + index + 1 : index;
                for (Tag copy : copies) {
                    if (collection.addTag(at, copy.copy())) {
                        at++;
                        changedHere = true;
                    }
                }
                changed += changedHere ? 1 : 0;
            }
            return changed;
        }

        public int remove(Tag tag) {
            List<Tag> parents = Collections.singletonList(tag);
            for (int i = 0; i < nodes.length - 1; i++) {
                parents = nodes[i].get(parents);
            }
            Node last = nodes[nodes.length - 1];
            int removed = 0;
            for (Tag parent : parents) {
                removed += last.removeTag(parent);
            }
            return removed;
        }

        private CommandSyntaxException notFound(Node node) {
            int end = nodeToOriginalPosition.getInt(node);
            return ERROR_NOTHING_FOUND.create(original.substring(0, end));
        }

        public String toString() {
            return original;
        }

        public String asString() {
            return original;
        }
    }

    interface Node {
        void getTag(Tag parent, List<Tag> output);

        void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output);

        Tag createPreferredParentTag();

        int setTag(Tag parent, Supplier<Tag> toAdd);

        int removeTag(Tag parent);

        default List<Tag> get(List<Tag> tags) {
            return collect(tags, this::getTag);
        }

        default List<Tag> getOrCreate(List<Tag> tags, Supplier<Tag> child) {
            return collect(tags, (parent, output) -> getOrCreateTag(parent, child, output));
        }

        default List<Tag> collect(List<Tag> tags, BiConsumer<Tag, List<Tag>> collector) {
            List<Tag> output = Lists.newArrayList();
            for (Tag tag : tags) {
                collector.accept(tag, output);
            }
            return output;
        }
    }

    static class CompoundChildNode implements Node {
        private final String name;

        public CompoundChildNode(String name) {
            this.name = name;
        }

        public void getTag(Tag parent, List<Tag> output) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found != null) {
                    output.add(found);
                }
            }
        }

        public void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found == null) {
                    found = child.get();
                    compound.put(name, found);
                }
                output.add(found);
            }
        }

        public Tag createPreferredParentTag() {
            return new CompoundTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            if (parent instanceof CompoundTag compound) {
                Tag value = toAdd.get();
                Tag previous = compound.put(name, value);
                return previous == null || !previous.equals(value) ? 1 : 0;
            }
            return 0;
        }

        public int removeTag(Tag parent) {
            if (parent instanceof CompoundTag compound && compound.contains(name)) {
                compound.remove(name);
                return 1;
            }
            return 0;
        }
    }

    static class IndexedElementNode implements Node {
        private final int index;

        public IndexedElementNode(int index) {
            this.index = index;
        }

        private int resolve(CollectionTag collection) {
            int resolved = index < 0 ? collection.size() + index : index;
            return resolved >= 0 && resolved < collection.size() ? resolved : -1;
        }

        public void getTag(Tag parent, List<Tag> output) {
            if (parent instanceof CollectionTag collection) {
                int at = resolve(collection);
                if (at >= 0) {
                    output.add(collection.get(at));
                }
            }
        }

        public void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output) {
            getTag(parent, output);
        }

        public Tag createPreferredParentTag() {
            return new ListTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            if (parent instanceof CollectionTag collection) {
                int at = resolve(collection);
                if (at >= 0) {
                    Tag previous = collection.get(at);
                    Tag value = toAdd.get();
                    if (!previous.equals(value) && collection.setTag(at, value)) {
                        return 1;
                    }
                }
            }
            return 0;
        }

        public int removeTag(Tag parent) {
            if (parent instanceof CollectionTag collection) {
                int at = resolve(collection);
                if (at >= 0) {
                    collection.remove(at);
                    return 1;
                }
            }
            return 0;
        }
    }

    static class MatchElementNode implements Node {
        private final CompoundTag pattern;
        private final Predicate predicate;

        public MatchElementNode(CompoundTag pattern) {
            this.pattern = pattern;
            this.predicate = matches(pattern);
        }

        public void getTag(Tag parent, List<Tag> output) {
            if (parent instanceof ListTag list) {
                for (Tag element : list) {
                    if (predicate.test(element)) {
                        output.add(element);
                    }
                }
            }
        }

        public void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output) {
            if (parent instanceof ListTag list) {
                int before = output.size();
                getTag(parent, output);
                if (output.size() == before) {
                    Tag created = pattern.copy();
                    list.addTag(list.size(), created);
                    output.add(created);
                }
            }
        }

        public Tag createPreferredParentTag() {
            return new ListTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            if (!(parent instanceof ListTag list)) {
                return 0;
            }
            int changed = 0;
            boolean found = false;
            for (int i = 0; i < list.size(); i++) {
                if (predicate.test(list.get(i))) {
                    found = true;
                    Tag value = toAdd.get();
                    if (!list.get(i).equals(value) && list.setTag(i, value)) {
                        changed++;
                    }
                }
            }
            if (!found) {
                list.addTag(list.size(), toAdd.get());
                changed++;
            }
            return changed;
        }

        public int removeTag(Tag parent) {
            if (!(parent instanceof ListTag list)) {
                return 0;
            }
            int removed = 0;
            for (int i = list.size() - 1; i >= 0; i--) {
                if (predicate.test(list.get(i))) {
                    list.remove(i);
                    removed++;
                }
            }
            return removed;
        }
    }

    static class AllElementsNode implements Node {
        public static final AllElementsNode INSTANCE = new AllElementsNode();

        private AllElementsNode() {}

        public void getTag(Tag parent, List<Tag> output) {
            if (parent instanceof CollectionTag collection) {
                for (Tag element : collection) {
                    output.add(element);
                }
            }
        }

        public void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output) {
            if (parent instanceof CollectionTag collection) {
                if (collection.isEmpty()) {
                    Tag created = child.get();
                    if (collection.addTag(0, created)) {
                        output.add(created);
                    }
                } else {
                    getTag(parent, output);
                }
            }
        }

        public Tag createPreferredParentTag() {
            return new ListTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            if (!(parent instanceof CollectionTag collection)) {
                return 0;
            }
            if (collection.isEmpty()) {
                return collection.addTag(0, toAdd.get()) ? 1 : 0;
            }
            int changed = 0;
            for (int i = 0; i < collection.size(); i++) {
                Tag value = toAdd.get();
                if (!collection.get(i).equals(value) && collection.setTag(i, value)) {
                    changed++;
                }
            }
            return changed;
        }

        public int removeTag(Tag parent) {
            if (parent instanceof CollectionTag collection && !collection.isEmpty()) {
                int size = collection.size();
                collection.clear();
                return size;
            }
            return 0;
        }
    }

    static class MatchObjectNode implements Node {
        private final String name;
        private final CompoundTag pattern;
        private final Predicate predicate;

        public MatchObjectNode(String name, CompoundTag pattern) {
            this.name = name;
            this.pattern = pattern;
            this.predicate = matches(pattern);
        }

        public void getTag(Tag parent, List<Tag> output) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found != null && predicate.test(found)) {
                    output.add(found);
                }
            }
        }

        public void getOrCreateTag(Tag parent, Supplier<Tag> child, List<Tag> output) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found == null) {
                    found = pattern.copy();
                    compound.put(name, found);
                    output.add(found);
                } else if (predicate.test(found)) {
                    output.add(found);
                }
            }
        }

        public Tag createPreferredParentTag() {
            return new CompoundTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found == null || predicate.test(found)) {
                    Tag value = toAdd.get();
                    Tag previous = compound.put(name, value);
                    return previous == null || !previous.equals(value) ? 1 : 0;
                }
            }
            return 0;
        }

        public int removeTag(Tag parent) {
            if (parent instanceof CompoundTag compound) {
                Tag found = compound.get(name);
                if (found != null && predicate.test(found)) {
                    compound.remove(name);
                    return 1;
                }
            }
            return 0;
        }
    }

    static class MatchRootObjectNode implements Node {
        private final Predicate predicate;

        public MatchRootObjectNode(CompoundTag pattern) {
            this.predicate = matches(pattern);
        }

        public void getTag(Tag self, List<Tag> output) {
            if (self instanceof CompoundTag && predicate.test(self)) {
                output.add(self);
            }
        }

        public void getOrCreateTag(Tag self, Supplier<Tag> child, List<Tag> output) {
            getTag(self, output);
        }

        public Tag createPreferredParentTag() {
            return new CompoundTag();
        }

        public int setTag(Tag parent, Supplier<Tag> toAdd) {
            return 0;
        }

        public int removeTag(Tag parent) {
            return 0;
        }
    }
}
