--[[
   @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
   ***** LINKYdelestage.lua - Delestage electrique avec module eeSmart en interface du compteur LINKY
   V0.11  - Decembre 2020 - Initial release
]]
    return {
        on = {
          devices = {
             'LINKY - L1/L2/L3 Draw'
          }
        },
        execute = function(domoticz, Current3phases)
           domoticz.log('LINKY - Verification non depassement abonnement 15kVA')    
           IntensiteDelestage=1.3*25
           if( tonumber( Current3phases.rawData[1] ) >= IntensiteDelestage or tonumber( Current3phases.rawData[2] ) >= IntensiteDelestage ) then
               domoticz.devices('Horaires/Stop Chauffage').switchSelector(30)
               domoticz.log('DELESTAGE LINKY phases 1 et 2 en raison de I1 = ' .. Current3phases.rawData[1] .. ' A et I2 = ' .. Current3phases.rawData[2] .. ' A')
           end    
           if( tonumber( Current3phases.rawData[3] ) >= IntensiteDelestage ) then
               domoticz.devices('Horaires/Stop Chauffage').switchSelector(50).afterSec(1) -- Delai pour assurer l'eventuelle execution d'une commande encore en cours de delestage pour I1 ou I2
               domoticz.log('DELESTAGE LINKY phase 3 en raison de I3 = ' .. Current3phases.rawData[3] .. ' A')
           end
        end
    }