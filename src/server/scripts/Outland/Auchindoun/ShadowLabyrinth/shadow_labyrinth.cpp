/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Unit.h"

enum Spells
{
    SPELL_MARK_OF_MALICE_TRIGGERED = 33494
};

// 33493 - Mark of Malice
class spell_mark_of_malice : public AuraScript
{
    PrepareAuraScript(spell_mark_of_malice);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MARK_OF_MALICE_TRIGGERED });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        // just drop charges
        if (aurEff->GetBase()->GetCharges() > 1)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_MARK_OF_MALICE_TRIGGERED, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_mark_of_malice::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_shadow_labyrinth()
{
    RegisterSpellScript(spell_mark_of_malice);
}
