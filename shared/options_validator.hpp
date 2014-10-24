#ifndef OPTIONS_VALIDATOR_HPP
#define OPTIONS_VALIDATOR_HPP

/**
 * Helper class for validating a boost::program_options::variables_map.
 *
 * Basic usage:
 * 	po::options_description descr;
 * 	po::variables_map vm;
 * 	descr.add_options()
 * 		("test", po::value<std::string>(), "Valid values: 'a', 'b', 'c' and 'q'");
 *
 *		// ...fill vm using normal procedure...
 *
 *		options_validator(vm, "test")("a")("b")("c")("q").validate();
 *
 *	If value test is set, but is not one of the defined values,
 *	we will throw po::validation_error with error po::validation_error::invalid_option_value
 *	for the specified key.
 */
namespace options_validator {


template<class T>
class options_validator {
private:
	std::vector<T> valid_values;
	const boost::program_options::variables_map &vm;
  	const std::string &key;
public:
	options_validator(const boost::program_options::variables_map &vm_, const std::string &key_)
		: vm(vm_), key(key_)
	{}

	/**
	 * Add a valid value of type T
	 */
	options_validator & operator() (const T &valid_value) {
		valid_values.push_back(valid_value);
		return *this;
	}

	/**
	 * Add all elements from the vector
	 */
	options_validator & operator() (const std::vector<T> &elems) {
		for(int i = 0; i < elems.size(); i++) {
			valid_values.push_back(elems[i]);
		}
		return *this;
	}

	/**
	 * Perform validation.
	 *
	 * If key is not present in variables_map, nothing happens.
	 * Else, go through every valid_value and compare using ==. 
	 *
	 * If no match is found, an exception is thrown
	 */
	void validate() {
		if(!vm.count(key))
			return;

		T value = vm[key].template as<T>();
		for (typename std::vector<T>::iterator it = valid_values.begin(); it != valid_values.end(); ++it)
		{
			if(*it == value) {
				return;
			}
		}

		throw boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value, key);
	}
};

}


#endif 
