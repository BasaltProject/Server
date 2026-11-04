// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.builder.ArgumentBuilder;
import com.mojang.brigadier.tree.ArgumentCommandNode;
import com.mojang.brigadier.tree.CommandNode;
import com.mojang.brigadier.tree.LiteralCommandNode;
import com.mojang.brigadier.tree.RootCommandNode;
import it.unimi.dsi.fastutil.ints.IntOpenHashSet;
import it.unimi.dsi.fastutil.ints.IntSet;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.commands.synchronization.ArgumentTypeInfos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;

public class ClientboundCommandsPacket implements Packet<ClientGamePacketListener> {
    private static final StreamCodec<FriendlyByteBuf, Entry> ENTRY = StreamCodec.of((output, entry) -> entry.write(output), ClientboundCommandsPacket::readNode);
    public static final StreamCodec<FriendlyByteBuf, ClientboundCommandsPacket> STREAM_CODEC = StreamCodec.composite(
        ENTRY.apply(ByteBufCodecs.list()), packet -> packet.entries,
        ByteBufCodecs.VAR_INT, packet -> packet.rootIndex,
        ClientboundCommandsPacket::new);
    private static final int kTypeMask = 3;
    private static final int kTypeLiteral = 1;
    private static final int kTypeArgument = 2;
    private static final int kExecutable = 4;
    private static final int kRedirect = 8;
    private static final int kCustomSuggestions = 16;
    private static final int kRestricted = 32;
    private final int rootIndex;
    private final List<Entry> entries;

    public <S> ClientboundCommandsPacket(RootCommandNode<S> root, NodeInspector<S> inspector) {
        Map<CommandNode<S>, Integer> ids = new IdentityHashMap<>();
        List<CommandNode<S>> order = new ArrayList<>();
        ArrayDeque<CommandNode<S>> queue = new ArrayDeque<>();
        queue.add(root);
        CommandNode<S> node;
        while ((node = queue.poll()) != null) {
            if (!ids.containsKey(node)) {
                ids.put(node, order.size());
                order.add(node);
                queue.addAll(node.getChildren());
                if (node.getRedirect() != null) {
                    queue.add(node.getRedirect());
                }
            }
        }
        this.entries = new ArrayList<>(order.size());
        for (CommandNode<S> each : order) {
            entries.add(entryOf(each, inspector, ids));
        }
        this.rootIndex = ids.get(root);
    }

    private ClientboundCommandsPacket(List<Entry> entries, int rootIndex) {
        this.entries = entries;
        this.rootIndex = rootIndex;
        validate(entries, Entry::canBuild);
        validate(entries, Entry::canResolve);
    }

    private static void validate(List<Entry> entries, java.util.function.BiPredicate<Entry, IntSet> check) {
        IntSet pending = new IntOpenHashSet();
        for (int i = 0; i < entries.size(); i++) {
            pending.add(i);
        }
        while (!pending.isEmpty()) {
            if (!pending.removeIf(index -> check.test(entries.get(index), pending))) {
                throw new IllegalStateException("Server sent an impossible command tree");
            }
        }
    }

    private static <S> Entry entryOf(CommandNode<S> node, NodeInspector<S> inspector, Map<CommandNode<S>, Integer> ids) {
        int flags = 0;
        int redirect = 0;
        if (node.getRedirect() != null) {
            flags |= kRedirect;
            redirect = ids.get(node.getRedirect());
        }
        if (inspector.isExecutable(node)) {
            flags |= kExecutable;
        }
        if (inspector.isRestricted(node)) {
            flags |= kRestricted;
        }
        NodeStub stub;
        if (node instanceof RootCommandNode<S>) {
            stub = null;
        } else if (node instanceof ArgumentCommandNode<S, ?> argument) {
            Identifier suggestion = inspector.suggestionId(argument);
            stub = new ArgumentNodeStub(argument.getName(), ArgumentTypeInfos.unpack(argument.getType()), suggestion);
            flags |= kTypeArgument | (suggestion != null ? kCustomSuggestions : 0);
        } else if (node instanceof LiteralCommandNode<S> literal) {
            stub = new LiteralNodeStub(literal.getLiteral());
            flags |= kTypeLiteral;
        } else {
            throw new UnsupportedOperationException("Unknown node type " + node);
        }
        int[] children = new int[node.getChildren().size()];
        int at = 0;
        for (CommandNode<S> child : node.getChildren()) {
            children[at++] = ids.get(child);
        }
        return new Entry(stub, flags, redirect, children);
    }

    private static Entry readNode(FriendlyByteBuf input) {
        byte flags = input.readByte();
        int[] children = input.readVarIntArray();
        int redirect = (flags & kRedirect) != 0 ? input.readVarInt() : 0;
        return new Entry(readStub(input, flags), flags, redirect, children);
    }

    private static NodeStub readStub(FriendlyByteBuf input, byte flags) {
        int type = flags & kTypeMask;
        if (type == kTypeArgument) {
            String name = input.readUtf();
            ArgumentTypeInfo<?, ?> info = ArgumentTypeInfos.byId(input.readVarInt());
            if (info == null) {
                return null;
            }
            ArgumentTypeInfo.Template<?> template = info.deserializeFromNetwork(input);
            Identifier suggestion = (flags & kCustomSuggestions) != 0 ? input.readIdentifier() : null;
            return new ArgumentNodeStub(name, template, suggestion);
        }
        return type == kTypeLiteral ? new LiteralNodeStub(input.readUtf()) : null;
    }

    public PacketType<ClientboundCommandsPacket> type() {
        return GamePacketTypes.CLIENTBOUND_COMMANDS;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleCommands(this);
    }

    public <S> RootCommandNode<S> getRoot(CommandBuildContext context, NodeBuilder<S> builder) {
        return (RootCommandNode<S>) new NodeResolver<S>(context, builder, entries).resolve(rootIndex);
    }

    static record LiteralNodeStub(String id) implements NodeStub {
        public <S> ArgumentBuilder<S, ?> build(CommandBuildContext context, NodeBuilder<S> builder) {
            return builder.createLiteral(id);
        }

        public void write(FriendlyByteBuf output) {
            output.writeUtf(id);
        }
    }

    static record ArgumentNodeStub(String id, ArgumentTypeInfo.Template<?> argumentType, Identifier suggestionId) implements NodeStub {
        public <S> ArgumentBuilder<S, ?> build(CommandBuildContext context, NodeBuilder<S> builder) {
            return builder.createArgument(id, argumentType.instantiate(context), suggestionId);
        }

        public void write(FriendlyByteBuf output) {
            output.writeUtf(id);
            writeType(output, argumentType);
            if (suggestionId != null) {
                output.writeIdentifier(suggestionId);
            }
        }

        @SuppressWarnings({"unchecked", "rawtypes"})
        private static void writeType(FriendlyByteBuf output, ArgumentTypeInfo.Template<?> template) {
            ArgumentTypeInfo info = template.type();
            output.writeVarInt(ArgumentTypeInfos.idOf(info));
            info.serializeToNetwork(template, output);
        }
    }

    static record Entry(NodeStub stub, int flags, int redirect, int[] children) {
        public void write(FriendlyByteBuf output) {
            output.writeByte(flags);
            output.writeVarIntArray(children);
            if ((flags & kRedirect) != 0) {
                output.writeVarInt(redirect);
            }
            if (stub != null) {
                stub.write(output);
            }
        }

        public boolean canBuild(IntSet unbuiltNodes) {
            return (flags & kRedirect) == 0 || !unbuiltNodes.contains(redirect);
        }

        public boolean canResolve(IntSet unresolvedNodes) {
            for (int child : children) {
                if (unresolvedNodes.contains(child)) {
                    return false;
                }
            }
            return true;
        }
    }

    static class NodeResolver<S> {
        private final CommandBuildContext context;
        private final NodeBuilder<S> builder;
        private final List<Entry> entries;
        private final List<CommandNode<S>> nodes;

        private NodeResolver(CommandBuildContext context, NodeBuilder<S> builder, List<Entry> entries) {
            this.context = context;
            this.builder = builder;
            this.entries = entries;
            this.nodes = new ArrayList<>(java.util.Collections.nCopies(entries.size(), null));
        }

        public CommandNode<S> resolve(int index) {
            CommandNode<S> known = nodes.get(index);
            if (known != null) {
                return known;
            }
            Entry entry = entries.get(index);
            CommandNode<S> result;
            if (entry.stub == null) {
                result = new RootCommandNode<>();
            } else {
                ArgumentBuilder<S, ?> made = entry.stub.build(context, builder);
                if ((entry.flags & kRedirect) != 0) {
                    made.redirect(resolve(entry.redirect));
                }
                result = builder.configure(made, (entry.flags & kExecutable) != 0, (entry.flags & kRestricted) != 0).build();
            }
            nodes.set(index, result);
            for (int childId : entry.children) {
                CommandNode<S> child = resolve(childId);
                if (!(child instanceof RootCommandNode)) {
                    result.addChild(child);
                }
            }
            return result;
        }
    }

    public interface NodeBuilder<S> {
        ArgumentBuilder<S, ?> createLiteral(String id);

        ArgumentBuilder<S, ?> createArgument(String id, ArgumentType<?> argumentType, Identifier suggestionId);

        ArgumentBuilder<S, ?> configure(ArgumentBuilder<S, ?> input, boolean executable, boolean restricted);
    }

    public interface NodeInspector<S> {
        Identifier suggestionId(ArgumentCommandNode<S, ?> node);

        boolean isExecutable(CommandNode<S> node);

        boolean isRestricted(CommandNode<S> node);
    }

    interface NodeStub {
        <S> ArgumentBuilder<S, ?> build(CommandBuildContext context, NodeBuilder<S> builder);

        void write(FriendlyByteBuf output);
    }
}
