package org.basaltmc.server.runtime

import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.Items
import org.basaltmc.wire.multiversion.SubstitutionLabels
import org.basaltmc.wire.text.Component

/** Names the stand-in sent for content a client is too old for, version included: `Cinnabar (26.2)`. */
internal class SubstituteLabels(
    private val version: String = BuildInfo.MINECRAFT_VERSION,
) : SubstitutionLabels {
    override fun itemLabel(nativeItemId: Int): Component? {
        val path =
            Items.REGISTRY
                .byId(nativeItemId)
                ?.id
                ?.path ?: return null
        return Component.text("${titleCase(path)} ($version)")
    }

    override fun entityLabel(nativeEntityTypeId: Int): Component? {
        val path = EntityTypes.byProtocolId(nativeEntityTypeId)?.id?.path ?: return null
        return Component.text("${titleCase(path)} ($version)")
    }

    /** `chiseled_cinnabar` reads as `Chiseled Cinnabar`: the client has no translation for it. */
    private fun titleCase(path: String): String =
        path
            .split('_')
            .filter { it.isNotEmpty() }
            .joinToString(" ") { word -> word.replaceFirstChar { it.uppercase() } }
}
