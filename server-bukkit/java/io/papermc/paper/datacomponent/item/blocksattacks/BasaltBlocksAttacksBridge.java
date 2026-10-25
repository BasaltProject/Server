package io.papermc.paper.datacomponent.item.blocksattacks;

import org.basaltmc.bukkit.datacomponent.ItemSpecs;

public final class BasaltBlocksAttacksBridge implements BlocksAttacksBridge {
    @Override
    public DamageReduction.Builder blocksAttacksDamageReduction() {
        return (DamageReduction.Builder) ItemSpecs.REDUCTION.builder();
    }

    @Override
    public ItemDamageFunction.Builder blocksAttacksItemDamageFunction() {
        return (ItemDamageFunction.Builder) ItemSpecs.ITEM_DAMAGE.builder();
    }
}
