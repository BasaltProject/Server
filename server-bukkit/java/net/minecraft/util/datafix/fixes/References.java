package net.minecraft.util.datafix.fixes;

import com.mojang.datafixers.DSL;

public class References {
    public static final DSL.TypeReference LEVEL = reference("level");
    public static final DSL.TypeReference PLAYER = reference("player");
    public static final DSL.TypeReference CHUNK = reference("chunk");
    public static final DSL.TypeReference BLOCK_ENTITY = reference("block_entity");
    public static final DSL.TypeReference ENTITY = reference("entity");
    public static final DSL.TypeReference ITEM_STACK = reference("item_stack");
    public static final DSL.TypeReference OPTIONS = reference("options");
    public static final DSL.TypeReference STRUCTURE = reference("structure");
    public static final DSL.TypeReference BLOCK_STATE = reference("block_state");
    public static final DSL.TypeReference BIOME = reference("biome");
    public static final DSL.TypeReference ITEM_NAME = reference("item_name");

    private References() {}

    private static DSL.TypeReference reference(String name) {
        return () -> name;
    }
}
