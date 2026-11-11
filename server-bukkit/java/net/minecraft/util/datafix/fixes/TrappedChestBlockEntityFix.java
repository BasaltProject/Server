// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class TrappedChestBlockEntityFix extends DataFix {
    public TrappedChestBlockEntityFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static final class TrappedChestSection extends LeavesFix.Section {
        private java.util.Set<Integer> chestIds;
        public TrappedChestSection(Typed<?> section, Schema inputSchema) {
            super(section, inputSchema);
        }
        protected boolean skippable() {
            chestIds = new java.util.HashSet<>();
            for (int i = 0; i < palette.size(); i++) {
                if ("minecraft:trapped_chest".equals(palette.get(i).get(NAME_TAG).asString(""))) {
                    chestIds.add(i);
                }
            }
            return chestIds.isEmpty();
        }
        public boolean isTrappedChest(int block) {
            return chestIds.contains(block);
        }
    }
}
