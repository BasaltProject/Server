// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import com.google.gson.JsonElement;
import com.mojang.serialization.JsonOps;
import java.util.Map;
import java.util.function.Function;
import net.minecraft.data.NativeData;
import net.minecraft.references.BlockItemId;
import net.minecraft.resources.Identifier;
import net.minecraft.tags.BlockItemTagId;
import net.minecraft.tags.TagEntry;
import net.minecraft.tags.TagFile;

public class VanillaBlockItemTagsProvider extends BlockItemTagsProvider {
    protected VanillaBlockItemTagsProvider(Function<BlockItemTagId, BlockItemTagsProvider.CombinedAppender> tagSupplier) {
        super(tagSupplier);
    }

    private static TagFile parse(JsonElement json) {
        return TagFile.CODEC.parse(JsonOps.INSTANCE, json).getOrThrow();
    }

    @Override
    protected void run() {
        Map<Identifier, JsonElement> blocks = NativeData.json("tags/block");
        Map<Identifier, JsonElement> items = NativeData.json("tags/item");
        blocks.forEach((id, blockJson) -> {
            if (!items.containsKey(id)) {
                return;
            }
            TagFile block = parse(blockJson);
            TagFile item = parse(items.get(id));
            CombinedAppender appender = tag(BlockItemTagId.create(id, id));
            for (TagEntry entry : block.entries()) {
                if (item.entries().stream().anyMatch(other -> other.toString().equals(entry.toString()))) {
                    Identifier target = Identifier.parse(entry.toString().replace("#", "").replace("?", ""));
                    if (entry.toString().startsWith("#")) {
                        appender.addTag(BlockItemTagId.create(target, target));
                    } else {
                        appender.add(BlockItemId.create(target, target));
                    }
                }
            }
        });
    }
}
