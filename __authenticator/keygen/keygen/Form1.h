#pragma once
#include <string>
#include "sha512.h"
#include <ctime>
#include <msclr/marshal_cppstd.h>
#include <time.h>
#include <algorithm>
namespace keygen {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Summary for Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}
		void SetTextBox1(String^ s) {
			this->textBox1->Text = s;
		}
	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::Label^ label1;
	private: System::Windows::Forms::Label^ label2;
	private: System::Windows::Forms::TextBox^ textBox1;
	private: System::Windows::Forms::TextBox^ textBox2;
	private: System::Windows::Forms::Label^ label3;
	protected:
	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->textBox1 = (gcnew System::Windows::Forms::TextBox());
			this->textBox2 = (gcnew System::Windows::Forms::TextBox());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->SuspendLayout();
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(37, 59);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(186, 13);
			this->label1->TabIndex = 0;
			this->label1->Text = L"Please enter your CHALLENGE CODE:";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Location = System::Drawing::Point(37, 134);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(183, 13);
			this->label2->TabIndex = 1;
			this->label2->Text = L"This is your your ACTIVATION CODE";
			// 
			// textBox1
			// 
			this->textBox1->Location = System::Drawing::Point(37, 75);
			this->textBox1->Name = L"textBox1";
			this->textBox1->Size = System::Drawing::Size(304, 20);
			this->textBox1->TabIndex = 2;
			this->textBox1->TextChanged += gcnew System::EventHandler(this, &Form1::textBox1_TextChanged);
			// 
			// textBox2
			// 
			this->textBox2->Location = System::Drawing::Point(37, 150);
			this->textBox2->Name = L"textBox2";
			this->textBox2->Size = System::Drawing::Size(304, 20);
			this->textBox2->TabIndex = 3;
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Location = System::Drawing::Point(94, 13);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(51, 13);
			this->label3->TabIndex = 4;
			this->label3->Text = L"KEYGEN";
			// 
			// MyForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(409, 261);
			this->Controls->Add(this->label3);
			this->Controls->Add(this->textBox2);
			this->Controls->Add(this->textBox1);
			this->Controls->Add(this->label2);
			this->Controls->Add(this->label1);
			this->Name = L"MyForm";
			this->Text = L"MyForm";
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
		private: System::Void textBox1_TextChanged(System::Object^ sender, System::EventArgs^ e) {
	
			String^ challenge;
			challenge = textBox1->Text;

			std::time_t rawtime;
			std::tm* timeinfo;
			char buffer[80];
			const int len = 5, parts = 2;

			srand(1);

			char srandChar[10];
			for (int i = 0; i < 10; i++) {
				srandChar[i]= rand() % 10;
			}
			std::string srandStr(srandChar);

			memset(&buffer[0], '\0', sizeof(buffer));
	
			std::time(&rawtime);
			timeinfo = std::localtime(&rawtime);

			std::strftime(buffer, 80, "%Y%m%d%H%M", timeinfo);
			std::string time(buffer);

			std::string challengeStr = msclr::interop::marshal_as< std::string >(challenge->ToString());

			std::transform(challengeStr.begin(), challengeStr.end(), challengeStr.begin(), ::toupper);




			std::string activation = sha512(srandStr + time + challengeStr);
		
			std::string activationShort = std::string(len * parts + parts - 1, '0');
			activationShort.clear();
			for (int i = 0; i < parts; ++i) {
				for (int j = 0; j < len; ++j) {
					activationShort.push_back(activation[len * i + j]);
				}
				activationShort.push_back('-');
			}
			activationShort.pop_back();
			std::transform(activationShort.begin(), activationShort.end(), activationShort.begin(), ::toupper);
			textBox2->Text =  gcnew String(activationShort.c_str());
		}
	};
}

