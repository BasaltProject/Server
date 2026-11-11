// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class EntityUUIDFix extends AbstractUUIDFix {
    public EntityUUIDFix(Schema outputSchema) {
        super(outputSchema, net.minecraft.util.datafix.fixes.References.ENTITY);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static Dynamic<?> updateLivingEntity(Dynamic<?> tag) {
        return modifiersFixed(tag);
    }
    public static Dynamic<?> updateEntityUUID(Dynamic<?> tag) {
        return replaceUUIDLeastMost(tag, "UUID", "UUID").orElse(tag);
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Dynamic<?> modifiersFixed(Dynamic tag) {
        return tag.update("Attributes", (java.util.function.Function<Dynamic, Dynamic>) attributes -> tag.createList(
            attributes.asStream().map(attribute -> ((Dynamic) attribute).update("Modifiers",
                (java.util.function.Function<Dynamic, Dynamic>) modifiers -> ((Dynamic) attribute).createList(modifiers.asStream()
                    .map(modifier -> (Dynamic) replaceUUIDLeastMost((Dynamic) modifier, "UUID", "UUID").orElse((Dynamic) modifier)))))));
    }
}
