--[[
   @Antori91  http://www.domoticz.com/forum/memberlist.php?mode=viewprofile&u=13749
   ***** Heating.lua - Régulation température RDC et 1er Mezzanine
   V0.10  - January 2021 - Initial release
]]

local dz = require ("global_data")

    return {
        on = {
           timer   = {'every 1 minutes'},
           devices = {'Horaires/Start Chauffage','Horaires/Stop Chauffage', dz.helpers.IDX_HEATINGSELECTOR, dz.helpers.IDX_THERMOSTAT_SETPOINT}
        },
        data = { 
           ChauffageZoneRDC = { initial = true }, -- Core House heating stopped (0) or not (1)
           TempMezza        = { initial = 1 },    -- Mezzanine temperature in relation to the setpoint and hysteresis gap: 0=below setpoint-Hysteresis,1=in Hysteresis Gap,2=over setpoint+Hysteresis
           TempRdc          = { initial = 1 }     -- Groundfloor temperature in relation to the setpoint and hysteresis gap: 0=below setpoint-Hysteresis,1=in Hysteresis Gap,2=over setpoint+Hysteresis
        },
        execute = function(domoticz, item)
           OffsetSetpointGroundFloor = -1
           Hysteresis                = 0.4
           VERBOSE                   = true      

           if( VERBOSE ) then domoticz.log( "Heating.lua V0.10 - Regulation ECO temperatures RDC et 1er Mezzanine") end
                      
           -- Check if heating rule regarding Core House has been changed by the schedule or a home user 
           if( item.name == 'Horaires/Start Chauffage' and domoticz.devices('Horaires/Start Chauffage').levelName == 'Rdc' and not domoticz.data.ChauffageZoneRDC ) then
               domoticz.data.ChauffageZoneRDC = true
               -- domoticz.devices('Horaires/Start Chauffage').switchSelector(110) -- Ack mode         
           end
           if( item.name == 'Horaires/Stop Chauffage' and domoticz.devices('Horaires/Stop Chauffage').levelName == 'Rdc' and domoticz.data.ChauffageZoneRDC ) then
               domoticz.data.ChauffageZoneRDC = false
               -- domoticz.devices('Horaires/Stop Chauffage').switchSelector(110) -- Ack mode         
           end

          
           -- Switch on/off heaters depending on temperature
           if( domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName == 'Eco' and domoticz.data.ChauffageZoneRDC ) then   -- If Heating mode is ECO et Groundfloor heating zone non stopped by a user or the heating schedule
               -- Température Mezzanine avec Internal Temp sensor temp/humidity device                
               if( domoticz.devices(domoticz.helpers.IDX_INDOORTEMPHUM).temperature > (domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint + Hysteresis) ) then
                   if( domoticz.data.TempMezza ~= 2 ) then
                       domoticz.log('Arret radiateurs Salon. Temp Mezzanine/Thermostat = ' .. string.format("%.1f", domoticz.devices(domoticz.helpers.IDX_INDOORTEMPHUM).temperature) .. '/' ..
                                    domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint .. ' degres')
                       domoticz.devices('Horaires/Stop Chauffage').switchSelector(30).afterSec(1)
                       domoticz.data.TempMezza = 2
                   end 
               elseif( domoticz.devices(domoticz.helpers.IDX_INDOORTEMPHUM).temperature <= (domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint - Hysteresis) ) then
                   if( domoticz.data.TempMezza ~= 0 ) then
                       domoticz.log('Demarrage radiateurs Salon. Temp Mezzanine/Thermostat = ' .. string.format("%.1f", domoticz.devices(domoticz.helpers.IDX_INDOORTEMPHUM).temperature) .. '/' .. 
                                    domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint .. ' degres')
                       domoticz.devices('Horaires/Start Chauffage').switchSelector(30).afterSec(1)
                       domoticz.data.TempMezza = 0
                   end 
               else domoticz.data.initialize('TempMezza') 
               end -- if( domoticz.devices(domoticz.helpers.IDX_INDOORTEMPHUM).temperature > (domoticz.devices(                  
               -- Température RDC avec Fire Alarm temp/humidity device
               if( domoticz.devices(domoticz.helpers.IDX_FIREALARMTEMPHUM).temperature > (domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint + OffsetSetpointGroundFloor + Hysteresis) ) then
                   if( domoticz.data.TempRdc ~= 2 ) then
                       domoticz.log('Arret radiateurs Entree/Cuisine/SalleAManger. Temp SalleAManger/Thermostat = ' .. string.format("%.1f", domoticz.devices(domoticz.helpers.IDX_FIREALARMTEMPHUM).temperature) .. '/' .. 
                                    domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint + OffsetSetpointGroundFloor .. ' degres')
                       domoticz.devices('Horaires/Stop Chauffage').switchSelector(10).afterSec(2)
                       domoticz.devices('Horaires/Stop Chauffage').switchSelector(20).afterSec(3) 
                       domoticz.data.TempRdc = 2
                   end 
               elseif( domoticz.devices(domoticz.helpers.IDX_FIREALARMTEMPHUM).temperature <= (domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint + OffsetSetpointGroundFloor - Hysteresis) ) then                    
                   if( domoticz.data.TempRdc ~= 0 ) then
                       domoticz.log('Demarrage radiateurs Entree/Cuisine/SalleAManger. Temp SalleAManger/Thermostat = ' .. string.format("%.1f", domoticz.devices(domoticz.helpers.IDX_FIREALARMTEMPHUM).temperature) .. '/' .. 
                                    domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint + OffsetSetpointGroundFloor .. ' degres')
                       domoticz.devices('Horaires/Start Chauffage').switchSelector(10).afterSec(2)
                       domoticz.devices('Horaires/Start Chauffage').switchSelector(20).afterSec(3)
                       domoticz.data.TempRdc = 0
                   end
               else domoticz.data.initialize('TempRdc') 
               end -- if( domoticz.devices(domoticz.helpers.IDX_FIREALARMTEMPHUM).temperature > (domoticz.devi  
           else
               domoticz.data.initialize('TempMezza')
               domoticz.data.initialize('TempRdc')
           end  --  if( domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName == 'Eco' and ChauffageZoneRDC == 1 )
           
           -- Log heating status
           if( VERBOSE ) then
               domoticz.log( "Reglage general du chauffage : " .. domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName )
               domoticz.log( "Thermostat sur : " .. domoticz.devices(domoticz.helpers.IDX_THERMOSTAT_SETPOINT).setPoint .. " degres" ) 
               if( ( (domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName == 'Eco') or (domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName == 'Confort') ) and domoticz.data.ChauffageZoneRDC ) then
                   if( domoticz.devices(domoticz.helpers.IDX_HEATINGSELECTOR).levelName == 'Eco' ) then 
                       domoticz.log( "ChauffageZoneRDC : En fonctionnement - Regulation mode ECO" ) 
                       domoticz.log( "Temperatures Mezzanine et Rdc : " .. domoticz.data.TempMezza .. "/" .. domoticz.data.TempRdc .. " (0=Sous la zone de consigne, 1=Dans la zone et 2=Au dessus)" )
                   else domoticz.log( "ChauffageZoneRDC : En fonctionnement - Sans regulation - mode CONFORT" ) end
               else domoticz.log( "ChauffageZoneRDC : Pas de chauffage" ) end --  -- if( domoticz.devices(17).levelName == 'Eco' or domoti             
               domoticz.log( "Horaires/Start Chauffage : " .. domoticz.devices('Horaires/Start Chauffage').levelName )
               domoticz.log( "Horaires/Stop  Chauffage : " .. domoticz.devices('Horaires/Stop Chauffage').levelName )                                            
           end -- if( VERBOSE ) then
             
        end  -- execute = function(domoticz, item)
    }