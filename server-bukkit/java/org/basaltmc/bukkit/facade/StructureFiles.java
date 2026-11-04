package org.basaltmc.bukkit.facade;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.basaltmc.bukkit.WorldNatives;

public final class StructureFiles {
    private StructureFiles() {}

    public static Path fileOf(ServerLevel level, Identifier name) {
        return level.getWorld().getWorldFolder().toPath().resolve("generated").resolve(name.getNamespace())
            .resolve("structures").resolve(name.getPath() + ".nbt");
    }

    private static ListTag ints(int a, int b, int c) {
        ListTag list = new ListTag();
        list.add(IntTag.valueOf(a));
        list.add(IntTag.valueOf(b));
        list.add(IntTag.valueOf(c));
        return list;
    }

    public static boolean exists(ServerLevel level, Identifier name) {
        return Files.isRegularFile(fileOf(level, name));
    }

    public static boolean save(ServerLevel level, Identifier name, BlockPos origin, Vec3i size, String author,
            List<Block> ignoreBlocks) {
        String world = level.getWorld().getName();
        Map<Integer, Integer> paletteIndex = new HashMap<>();
        ListTag palette = new ListTag();
        ListTag blocks = new ListTag();
        for (int y = 0; y < size.getY(); y++) {
            for (int z = 0; z < size.getZ(); z++) {
                for (int x = 0; x < size.getX(); x++) {
                    BlockState state = level.getBlockState(origin.offset(x, y, z));
                    if (ignoreBlocks != null && ignoreBlocks.contains(state.getBlock())) {
                        continue;
                    }
                    Integer index = paletteIndex.get(state.stateId());
                    if (index == null) {
                        index = palette.size();
                        paletteIndex.put(state.stateId(), index);
                        palette.add(stateTag(state));
                    }
                    CompoundTag entry = new CompoundTag();
                    entry.put("pos", ints(x, y, z));
                    entry.putInt("state", index);
                    byte[] raw = WorldNatives.blockEntityNbt(world, origin.getX() + x, origin.getY() + y,
                        origin.getZ() + z);
                    CompoundTag tile = raw == null ? null : NbtCodec.fromBytes(raw);
                    if (tile != null) {
                        tile.remove("x");
                        tile.remove("y");
                        tile.remove("z");
                        entry.put("nbt", tile);
                    }
                    blocks.add(entry);
                }
            }
        }
        CompoundTag root = new CompoundTag();
        root.putInt("DataVersion", WorldNatives.dataVersion());
        root.put("size", ints(size.getX(), size.getY(), size.getZ()));
        root.put("palette", palette);
        root.put("blocks", blocks);
        root.put("entities", new ListTag());
        if (author != null && !author.isEmpty()) {
            root.putString("author", author);
        }
        try {
            Path file = fileOf(level, name);
            Files.createDirectories(file.getParent());
            NbtIo.writeCompressed(root, file);
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    private static CompoundTag stateTag(BlockState state) {
        String text = WorldNatives.stateText(state.stateId());
        CompoundTag tag = new CompoundTag();
        int bracket = text.indexOf('[');
        tag.putString("Name", bracket < 0 ? text : text.substring(0, bracket));
        if (bracket >= 0) {
            CompoundTag props = new CompoundTag();
            for (String pair : text.substring(bracket + 1, text.length() - 1).split(",")) {
                int eq = pair.indexOf('=');
                props.putString(pair.substring(0, eq), pair.substring(eq + 1));
            }
            tag.put("Properties", props);
        }
        return tag;
    }

    private static String stateText(CompoundTag tag) {
        StringBuilder text = new StringBuilder(tag.getStringOr("Name", "minecraft:air"));
        CompoundTag props = tag.getCompoundOrEmpty("Properties");
        if (!props.isEmpty()) {
            text.append('[');
            boolean first = true;
            for (String key : props.keySet()) {
                text.append(first ? "" : ",").append(key).append('=').append(props.getStringOr(key, ""));
                first = false;
            }
            text.append(']');
        }
        return text.toString();
    }

    public static Vec3i sizeOf(CompoundTag root) {
        ListTag size = root.getListOrEmpty("size");
        return new Vec3i(size.getIntOr(0, 0), size.getIntOr(1, 0), size.getIntOr(2, 0));
    }

    public static CompoundTag read(ServerLevel level, Identifier name) {
        try {
            return NbtIo.readCompressed(fileOf(level, name), NbtAccounter.unlimitedHeap());
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    private static int[] transform(int x, int z, Mirror mirror, Rotation rotation) {
        if (mirror == Mirror.LEFT_RIGHT) {
            z = -z;
        } else if (mirror == Mirror.FRONT_BACK) {
            x = -x;
        }
        return switch (rotation) {
            case CLOCKWISE_90 -> new int[] {-z, x};
            case CLOCKWISE_180 -> new int[] {-x, -z};
            case COUNTERCLOCKWISE_90 -> new int[] {z, -x};
            default -> new int[] {x, z};
        };
    }

    public static int place(ServerLevel level, CompoundTag root, BlockPos origin, Mirror mirror, Rotation rotation,
            float integrity, long seed) {
        String world = level.getWorld().getName();
        ListTag palette = root.getListOrEmpty("palette");
        int[] states = new int[palette.size()];
        for (int i = 0; i < states.length; i++) {
            int id = WorldNatives.stateId(stateText(palette.getCompoundOrEmpty(i)));
            int turned = id < 0 ? id : WorldEnvNatives.transformState(id, rotation.ordinal(), mirror.ordinal());
            states[i] = turned < 0 ? id : turned;
        }
        ListTag blocks = root.getListOrEmpty("blocks");
        List<int[]> positions = new ArrayList<>();
        List<CompoundTag> tiles = new ArrayList<>();
        Random random = new Random(seed);
        int[] flat = new int[blocks.size() * 3];
        int[] placed = new int[blocks.size()];
        int count = 0;
        for (int i = 0; i < blocks.size(); i++) {
            CompoundTag entry = blocks.getCompoundOrEmpty(i);
            ListTag pos = entry.getListOrEmpty("pos");
            int state = states[entry.getIntOr("state", 0)];
            if (state < 0 || (integrity < 1.0F && random.nextFloat() > integrity)) {
                continue;
            }
            int[] turned = transform(pos.getIntOr(0, 0), pos.getIntOr(2, 0), mirror, rotation);
            flat[count * 3] = origin.getX() + turned[0];
            flat[count * 3 + 1] = origin.getY() + pos.getIntOr(1, 0);
            flat[count * 3 + 2] = origin.getZ() + turned[1];
            placed[count] = state;
            count++;
            CompoundTag tile = entry.getCompound("nbt").orElse(null);
            tiles.add(tile);
        }
        WorldNatives.writeStates(world, java.util.Arrays.copyOf(flat, count * 3), java.util.Arrays.copyOf(placed, count),
            true);
        for (int i = 0; i < count; i++) {
            CompoundTag tile = tiles.get(i);
            if (tile != null) {
                tile.putInt("x", flat[i * 3]);
                tile.putInt("y", flat[i * 3 + 1]);
                tile.putInt("z", flat[i * 3 + 2]);
                WorldNatives.setBlockEntityNbt(world, flat[i * 3], flat[i * 3 + 1], flat[i * 3 + 2],
                    NbtCodec.toBytes(tile));
            }
        }
        return count;
    }
}
