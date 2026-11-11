// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;

public class BlockListReport implements DataProvider {
    private final PackOutput output;

    public BlockListReport(PackOutput output, CompletableFuture<HolderLookup.Provider> registries) {
        this.output = output;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = new JsonObject();
        for (Block block : BuiltInRegistries.BLOCK) {
            Identifier id = BuiltInRegistries.BLOCK.getKey(block);
            JsonObject entry = new JsonObject();
            JsonObject properties = new JsonObject();
            for (Property<?> property : block.getStateDefinition().getProperties()) {
                JsonArray values = new JsonArray();
                for (Comparable value : property.getPossibleValues()) {
                    values.add(((Property) property).getName(value));
                }
                properties.add(property.getName(), values);
            }
            if (!properties.isEmpty()) {
                entry.add("properties", properties);
            }
            JsonArray states = new JsonArray();
            for (BlockState state : block.getStateDefinition().getPossibleStates()) {
                JsonObject stateJson = new JsonObject();
                stateJson.addProperty("id", Block.getId(state));
                if (state == block.defaultBlockState()) {
                    stateJson.addProperty("default", true);
                }
                JsonObject values = new JsonObject();
                for (Property<?> property : block.getStateDefinition().getProperties()) {
                    values.addProperty(property.getName(), ((Property) property).getName(state.getValue(property)));
                }
                if (!values.isEmpty()) {
                    stateJson.add("properties", values);
                }
                states.add(stateJson);
            }
            entry.add("states", states);
            root.add(id.toString(), entry);
        }
        return ReportJson.write(cache, output, "blocks.json", root);
    }

    @Override
    public final String getName() {
        return "Block List";
    }
}
