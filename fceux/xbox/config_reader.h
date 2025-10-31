#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <xtl.h>

#include "simpleIni.h"
#define E_CONFIG_SECTION_NOT_FOUND 0x1000
#define E_CONFIG_KEY_NOT_FOUND 0x1005
#define E_FILE_NOT_FOUND 0x2000


class Config{
private:
	//Array contenant la configuration
	typedef std::map<std::string, std::string> Config_k_v_map;	
	typedef std::map<std::string, Config_k_v_map> Sections_map;	
	//Config_k_v_map mapConfig;

	Sections_map m_config;
public:	
	HRESULT Find(std::string section, std::string key, std::string & value){
		Sections_map::iterator it;
		
		//recherche
		it=m_config.find(section);
		
		//Pas trouvé
		if(it == m_config.end())
		{
			return E_CONFIG_SECTION_NOT_FOUND;
		}
		else
		{
			//copie la valeur
			//value = it->second;
			Config_k_v_map mapConfig = it->second;
			Config_k_v_map::iterator cit;

			cit=mapConfig.find(key);
			
			//pas trouvé
			if(cit == mapConfig.end())
			{
				return E_CONFIG_SECTION_NOT_FOUND;
			}
			else
			{
				value = cit->second;
			}
			
			return S_OK;
		}
	};

	HRESULT Find(std::string section, std::string key, unsigned long & value){
		if(key.length()){
			std::string buffer;
			if(SUCCEEDED(Find(section, key,buffer)))
			{	
				value = atoi (buffer.c_str());
				return S_OK;
			}
			return E_CONFIG_SECTION_NOT_FOUND;
		}
		else
			return E_FAIL;
	};

	HRESULT Set(std::string section, std::string key, std::string value){
		if(section.length())
		{
			if(key.length()){
				Config_k_v_map mapConfig;
				if(m_config.find(section.c_str()) != m_config.end())
					mapConfig = m_config.find(section.c_str())->second;

				mapConfig[key]=value;
				m_config[section]=mapConfig;
				return S_OK;
			}
		}

		return E_FAIL;
	};

	HRESULT Set(std::string section, std::string key, unsigned long  value){
		char buffer [33];//32+1
		sprintf(buffer,"%d",value);
		return Set(section,key,buffer);
	};

	HRESULT Find(std::string section, std::string key, int & value){
		return Find(section, key,(unsigned long &)value);
	};


	HRESULT Save(std::string filename){	
		//parse each line
		CSimpleIniA ini;
		SI_Error rc;

		Sections_map::iterator jit;
		for ( jit=m_config.begin() ; jit != m_config.end(); jit++ )
		{
			Config_k_v_map mapConfig = jit->second;
			Config_k_v_map::iterator it;
			//sauvegarde tous les membres
			for ( it=mapConfig.begin() ; it != mapConfig.end(); it++ )
			{
				//save each line
				rc = ini.SetValue(jit->first.c_str(), (*it).first.c_str(), (*it).second.c_str());
				if(rc<0)
					return rc;
			}
		}
		rc = ini.SaveFile(filename.c_str());
		if(rc<0)
			return E_FILE_NOT_FOUND;

		return S_OK;
	};
	
	HRESULT Load(std::string filename){
		//parse each line
		CSimpleIniA ini;
				
		if (ini.LoadFile(filename.c_str()) < 0)
		{
			return E_FILE_NOT_FOUND;
		}

		CSimpleIniA::TNamesDepend::const_iterator it;
		CSimpleIniA::TNamesDepend names;
		ini.GetAllSections(names);

		for(it = names.begin(); it != names.end(); ++it)
		{
			const CSimpleIniA::TKeyVal * kval = ini.GetSection(it->pItem);
			
			if(kval==NULL)
				return E_FAIL;

			Config_k_v_map mapConfig;

			//iterate all keys
			CSimpleIniA::TKeyVal::const_iterator i;
			for (i = kval->begin(); i != kval->end(); ++i) 
			{ 
				
				const char * key = i->first.pItem;
				const char * val = i->second;
				mapConfig[key]=val;
			}
			m_config[it->pItem]=mapConfig;
		}
		return S_OK;
	};
};

