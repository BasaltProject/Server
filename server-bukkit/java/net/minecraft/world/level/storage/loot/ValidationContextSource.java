// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.ProblemReporter;
import net.minecraft.util.context.ContextKeySet;
public class ValidationContextSource {
    private final ProblemReporter reporter;
    private final HolderGetter.Provider lootData;
    private ValidationContext cachedEntity;
    public ValidationContextSource(ProblemReporter reporter, HolderGetter.Provider lootData) {
        this.reporter = reporter;
        this.lootData = lootData;
        
    }
    public ValidationContext context(ContextKeySet params) {
        return new ValidationContext(reporter, params, lootData);
    }
    public ValidationContext entityContext() {
        if (cachedEntity == null) {
            cachedEntity = context(LootContextParamSets.ADVANCEMENT_ENTITY);
        }
        return cachedEntity;
    }
}
