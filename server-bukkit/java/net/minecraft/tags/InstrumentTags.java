// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.Instrument;

public interface InstrumentTags {
    TagKey<Instrument> REGULAR_GOAT_HORNS = org.basaltmc.bukkit.facade.Tags.key(Registries.INSTRUMENT, "regular_goat_horns");
    TagKey<Instrument> SCREAMING_GOAT_HORNS = org.basaltmc.bukkit.facade.Tags.key(Registries.INSTRUMENT, "screaming_goat_horns");
    TagKey<Instrument> GOAT_HORNS = org.basaltmc.bukkit.facade.Tags.key(Registries.INSTRUMENT, "goat_horns");
}
