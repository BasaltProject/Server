// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.List;
import java.util.Map;
public class StructuresBecomeConfiguredFix extends DataFix {
    public StructuresBecomeConfiguredFix(Schema outputSchema) {
        super(outputSchema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    static record Conversion(Map<String, String> biomeMapping, String fallback) {
        public static Conversion trivial(String result) {
            return new Conversion(Map.of(), result);
        }
        public static Conversion biomeMapped(Map<List<String>, String> mapping, String fallback) {
            Map<String, String> flat = new java.util.HashMap<>();
            mapping.forEach((biomes, result) -> biomes.forEach(biome -> flat.put(biome, result)));
            return new Conversion(flat, fallback);
        }
    }
}
